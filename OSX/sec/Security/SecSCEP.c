/*
 * Copyright (c) 2008-2010,2012-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "SecSCEP.h"

#include <Security/SecCMS.h>
#include <Security/SecRandom.h>
#include <Security/SecIdentityPriv.h>
#include <string.h>
#include <AssertMacros.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecIdentity.h>
#include <Security/SecPolicy.h>
#include <libDER/DER_Encode.h>
#include <uuid/uuid.h>
#include <utilities/array_size.h>
#include <utilities/debugging.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecCFWrappers.h>

typedef enum {
        messageType = 2,
        pkiStatus = 3,
        failInfo = 4,
        senderNonce = 5,
        recipientNonce = 6,
        transId = 7
} scep_attr_t;

static CF_RETURNS_RETAINED CFDataRef scep_oid(scep_attr_t type)
{
/* +-------------------+-----------------------------------------------+
   | Name              | ASN.1 Definition                              |
   +-------------------+-----------------------------------------------+
   | id-VeriSign       | OBJECT_IDENTIFIER ::= {2 16 US(840) 1         |
   |                   | VeriSign(113733)}                             |
   | id-pki            | OBJECT_IDENTIFIER ::= {id-VeriSign pki(1)}    |
   | id-attributes     | OBJECT_IDENTIFIER ::= {id-pki attributes(9)}  |
   | id-messageType    | OBJECT_IDENTIFIER ::= {id-attributes          |
   |                   | messageType(2)}                               |
   | id-pkiStatus      | OBJECT_IDENTIFIER ::= {id-attributes          |
   |                   | pkiStatus(3)}                                 |
   | id-failInfo       | OBJECT_IDENTIFIER ::= {id-attributes          |
   |                   | failInfo(4)}                                  |
   | id-senderNonce    | OBJECT_IDENTIFIER ::= {id-attributes          |
   |                   | senderNonce(5)}                               |
   | id-recipientNonce | OBJECT_IDENTIFIER ::= {id-attributes          |
   |                   | recipientNonce(6)}                            |
   | id-transId        | OBJECT_IDENTIFIER ::= {id-attributes          |
   |                   | transId(7)}                                   |
   | id-extensionReq   | OBJECT_IDENTIFIER ::= {id-attributes          |
   |                   | extensionReq(8)}                              |
   +-------------------+-----------------------------------------------+ */
    uint8_t oid_scep_attrs[] = 
        { 0x60, 0x86, 0x48, 0x01, 0x86, 0xF8, 0x45, 0x01, 0x09, 0 };
    /* messageType:2 pkiStatus:3 failInfo:4 senderNonce:5 recipientNonce:6 transId:7 */
    if ((type < messageType) || (type > transId))
        return NULL;
        
    oid_scep_attrs[sizeof(oid_scep_attrs) - 1] = type;
    return CFDataCreate(kCFAllocatorDefault, oid_scep_attrs, sizeof(oid_scep_attrs));
}

static const char CertRep[] = "3";
static const char PKCSReq[] = "19";
static const char GetCertInitial[] = "20";
__unused static const char GetCert[] = "21";
__unused static const char GetCRL[] = "22";
static const char PKIStatusSUCCESS[] = "0";
__unused static const char PKIStatusFAILURE[] = "2";
static const char PKIStatusPENDING[] = "3";

static CF_RETURNS_RETAINED CFDataRef
printable_string_data(size_t length, const char *bytes)
{
    DERSize der_length_len = DERLengthOfLength(length);
    size_t value_length = sizeof(SecASN1PrintableString) + der_length_len + length;
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, value_length);
    CFDataSetLength(data, value_length);
    uint8_t *ptr = (uint8_t *)CFDataGetBytePtr(data);
    *ptr++ = SecASN1PrintableString;
    DEREncodeLength(length, ptr, &der_length_len);
    ptr += der_length_len;
    memcpy(ptr, bytes, length);
    return (CFDataRef)data;
}

#define scep_result(value) printable_string_data(sizeof(value)-1, value)

static CF_RETURNS_NOT_RETAINED CFTypeRef
dictionary_array_value_1(CFDictionaryRef attrs, CFTypeRef attr)
{
    CFTypeRef value = NULL;
    CFArrayRef attr_values = NULL;
    
    require(attr_values = (CFArrayRef)CFDictionaryGetValue(attrs, attr), out);
    require(CFArrayGetCount(attr_values) == 1, out);
    value = CFArrayGetValueAtIndex(attr_values, 0);
out:
    return value;
}

/* @@@ consider splitting into function returning single value 
       and function creating printable string from c str */
static bool scep_attr_has_val(CFDictionaryRef attrs, scep_attr_t attr, const char *val)
{
    bool result = false;
    CFDataRef msgtype_value_data = printable_string_data(strlen(val), val);
    CFArrayRef msgtype_value_datas = CFArrayCreate(kCFAllocatorDefault, 
        (const void **)&msgtype_value_data, 1, &kCFTypeArrayCallBacks);
    CFRelease(msgtype_value_data);
    CFDataRef msgtype_oid_data = scep_oid(attr);
    CFArrayRef msgtype_values = (CFArrayRef)CFDictionaryGetValue(attrs, msgtype_oid_data);
    CFRelease(msgtype_oid_data);
    if (msgtype_values && CFEqual(msgtype_value_datas, msgtype_values))
        result = true;
    CFRelease(msgtype_value_datas);

    return result;
}

static CF_RETURNS_RETAINED CFDataRef hexencode(CFDataRef data)
{
    CFIndex ix, length = CFDataGetLength(data);
    const uint8_t *bin_data = CFDataGetBytePtr(data);
    uint8_t *hex_data = calloc(1, 2*length + 1);
    require(length && bin_data && hex_data, out);

    for (ix = 0; ix < length; ix++)
        snprintf((char *)&hex_data[2*ix], 3, "%02X", bin_data[ix]);

    return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, hex_data, 
        2*length, kCFAllocatorMalloc);
out:
    if (hex_data)
        free(hex_data);
    return NULL;
}

static CF_RETURNS_RETAINED CFDataRef pubkeyhash(SecKeyRef key)
{
    CFTypeRef key_type = NULL;
    CFDictionaryRef pubkey_attrs = NULL;
    CFDataRef hash_pubkey_data = NULL, pubkey_data = NULL;
    uint8_t pubkey_hash[CC_SHA1_DIGEST_LENGTH];
    
    require(pubkey_attrs = SecKeyCopyAttributeDictionary(key), out);
    require( (key_type = CFDictionaryGetValue(pubkey_attrs, kSecAttrKeyClass)) &&
                CFEqual(key_type, kSecAttrKeyClassPublic), out);
    require(pubkey_data = CFDictionaryGetValue(pubkey_attrs, kSecValueData), out);
    require((unsigned long)CFDataGetLength(pubkey_data)<=UINT32_MAX, out); /* Correct as long as CFIndex is long */
    CCDigest(kCCDigestSHA1, CFDataGetBytePtr(pubkey_data), (CC_LONG)CFDataGetLength(pubkey_data), pubkey_hash);
    hash_pubkey_data = CFDataCreate(kCFAllocatorDefault, pubkey_hash, sizeof(pubkey_hash));
out:
    CFReleaseSafe(pubkey_attrs);
    return hash_pubkey_data;
}

static int generate_sender_nonce(CFMutableDictionaryRef dict)
{
    /* random sender nonce, to be verified against recipient nonce in reply */
    CFDataRef senderNonce_oid_data = scep_oid(senderNonce);
    uint8_t senderNonce_value[18] = { 4, 16, };
    int status = SecRandomCopyBytes(kSecRandomDefault, sizeof(senderNonce_value) - 2, senderNonce_value + 2);
    CFDataRef senderNonce_value_data = CFDataCreate(kCFAllocatorDefault,
		senderNonce_value, sizeof(senderNonce_value));
	if (senderNonce_oid_data && senderNonce_value_data)
		CFDictionarySetValue(dict, senderNonce_oid_data, senderNonce_value_data);
    CFReleaseNull(senderNonce_oid_data);
    CFReleaseNull(senderNonce_value_data);
    return status;
}

SecIdentityRef SecSCEPCreateTemporaryIdentity(SecKeyRef publicKey, SecKeyRef privateKey)
{
	int key_usage = kSecKeyUsageDigitalSignature | kSecKeyUsageKeyEncipherment;
	CFDictionaryRef self_signed_parameters = NULL;
	CFNumberRef key_usage_num = NULL;
	SecCertificateRef self_signed_certificate = NULL;
	SecIdentityRef self_signed_identity = NULL;
	CFStringRef cn_uuid = NULL;
	CFArrayRef cn_dn = NULL, cn_dns = NULL, unique_rdns = NULL;

	key_usage_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_usage);
	require(key_usage_num, out);

	const void *key[] = { kSecCertificateKeyUsage };
	const void *val[] = { key_usage_num };
	self_signed_parameters = CFDictionaryCreate(kCFAllocatorDefault,
	    key, val, array_size(key),
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	require(self_signed_parameters, out);

	char uuid_string[37] = {};
	uuid_t uuid;
	uuid_generate_random(uuid);
	uuid_unparse(uuid, uuid_string);
	cn_uuid = CFStringCreateWithCString(kCFAllocatorDefault, uuid_string, kCFStringEncodingASCII);
	require(cn_uuid, out);
	const void * cn[] = { kSecOidCommonName, cn_uuid };
	cn_dn = CFArrayCreate(kCFAllocatorDefault, cn, 2, NULL);
	require(cn_dn, out);
	cn_dns = CFArrayCreate(kCFAllocatorDefault, (const void **)&cn_dn, 1, NULL);
	require(cn_dns, out);
	unique_rdns = CFArrayCreate(kCFAllocatorDefault, (const void **)&cn_dns, 1, NULL);
	require(unique_rdns, out);

	self_signed_certificate = SecGenerateSelfSignedCertificate(unique_rdns, self_signed_parameters, publicKey, privateKey);
	require(self_signed_certificate, out);
	self_signed_identity = SecIdentityCreate(kCFAllocatorDefault, self_signed_certificate, privateKey);

out:
	CFReleaseSafe(key_usage_num);
	CFReleaseSafe(self_signed_parameters);
	CFReleaseSafe(self_signed_certificate);
	CFReleaseSafe(unique_rdns);
	CFReleaseSafe(cn_dns);
	CFReleaseSafe(cn_dn);
	CFReleaseSafe(cn_uuid);

	return self_signed_identity;
}

static CF_RETURNS_RETAINED CFTypeRef filterRecipients(CFTypeRef recipients)
{
    if (recipients && SecCertificateGetTypeID() == CFGetTypeID(recipients)) {
        return CFRetainSafe(recipients);
    } else if (isArray(recipients)) {
        CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        CFArrayForEach(recipients, ^(const void *value) {
            if (SecCertificateGetTypeID() != CFGetTypeID(value)) {
                return;
            }
            SecCertificateRef cert = (SecCertificateRef)value;
            SecKeyUsage keyUsage = SecCertificateGetKeyUsage(cert);
            // No Key Usage specified or KeyEncipherment bit set
            if (keyUsage == kSecKeyUsageUnspecified || ((keyUsage & kSecKeyUsageKeyEncipherment) != 0)) {
                CFArrayAppendValue(result, cert);
            }
        });
        // If we didn't find any encryption certificates in the array, just use the full list of recipients
        if (CFArrayGetCount(result) == 0) {
            CFReleaseNull(result);
            return CFRetainSafe(recipients);
        }
        return result;
    }
    return NULL;
}

CFDataRef
SecSCEPGenerateCertificateRequest(CFArrayRef subject, CFDictionaryRef parameters,
    SecKeyRef publicKey, SecKeyRef privateKey,
    SecIdentityRef signer, CFTypeRef recipients)
{
    CFDataRef csr = NULL;
    CFMutableDataRef enveloped_data = NULL;
    CFMutableDictionaryRef simple_attr = NULL;
    SecIdentityRef self_signed_identity = NULL;
    CFMutableDataRef signed_request = NULL;
    SecCertificateRef recipient = NULL;
    CFDataRef msgtype_value_data = NULL;
    CFDataRef msgtype_oid_data = NULL;
    SecKeyRef realPublicKey = NULL;
    SecKeyRef recipientKey = NULL;

    CFTypeRef filteredRecipients = filterRecipients(recipients);
    if (CFGetTypeID(filteredRecipients) == SecCertificateGetTypeID()) {
        recipient = (SecCertificateRef)filteredRecipients;
    } else if (CFGetTypeID(filteredRecipients) == CFArrayGetTypeID()) {
        recipient = (SecCertificateRef)CFArrayGetValueAtIndex(filteredRecipients, 0);
    }
    require(recipient, out);

    /* We don't support EC recipients for SCEP yet. */
    recipientKey = SecCertificateCopyKey(recipient);
    require(SecKeyGetAlgorithmId(recipientKey) == kSecRSAAlgorithmID, out);

    realPublicKey = SecKeyCopyPublicKey(privateKey);
    if (!realPublicKey) {
        /* If we can't get the public key from the private key,
         * fall back to the public key provided by the caller. */
        realPublicKey = CFRetainSafe(publicKey);
    }
    require(realPublicKey, out);
    require(csr = SecGenerateCertificateRequest(subject, parameters, realPublicKey, privateKey), out);
    require(enveloped_data = CFDataCreateMutable(kCFAllocatorDefault, 0), out);
    require_noerr(SecCMSCreateEnvelopedData(recipient, parameters, csr, enveloped_data), out);
    CFReleaseNull(csr);

    simple_attr = CFDictionaryCreateMutable(kCFAllocatorDefault, 3, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    /* generate a transaction id: hex encoded pubkey hash */
    CFDataRef public_key_hash = pubkeyhash(realPublicKey);
    CFDataRef public_key_hash_hex = hexencode(public_key_hash);
    CFReleaseSafe(public_key_hash);
    CFDataRef transid_oid_data = scep_oid(transId);
    CFDataRef transid_data = printable_string_data(CFDataGetLength(public_key_hash_hex), 
        (const char *)CFDataGetBytePtr(public_key_hash_hex));
    CFReleaseSafe(public_key_hash_hex);
    
    CFDictionarySetValue(simple_attr, transid_oid_data, transid_data);
    CFReleaseNull(transid_oid_data);
    CFReleaseNull(transid_data);
    
    /* message type: PKCSReq (19) */
    msgtype_value_data = NULL;
    msgtype_oid_data = NULL;
    require(msgtype_oid_data = scep_oid(messageType), out);
    require(msgtype_value_data = printable_string_data(strlen(PKCSReq), PKCSReq), out);

    CFDictionarySetValue(simple_attr, msgtype_oid_data, msgtype_value_data);
    CFReleaseNull(msgtype_oid_data);
    CFReleaseNull(msgtype_value_data);

    /* random sender nonce, to be verified against recipient nonce in reply */
	require(generate_sender_nonce(simple_attr) == errSecSuccess, out);

	/* XXX/cs remove auto-generation once managedconfig is no longer using this */
    if (signer) {
        self_signed_identity = signer;
        CFRetain(self_signed_identity);
    } else {
		self_signed_identity = SecSCEPCreateTemporaryIdentity(realPublicKey, privateKey);

        /* Add our temporary cert to the keychain for CMS decryption of
           the reply.  If we happened to have picked an existing UUID
           we fail.  We should pick a different UUID and try again. */
        require(self_signed_identity, out);
        CFDictionaryRef identity_add = CFDictionaryCreate(NULL, 
            (const void **)&kSecValueRef, (const void **)&self_signed_identity, 1, NULL, NULL);
        require_noerr_action(SecItemAdd(identity_add, NULL), out,
            CFReleaseSafe(identity_add));
        CFReleaseSafe(identity_add);
    }
    require(self_signed_identity, out);

    signed_request = CFDataCreateMutable(kCFAllocatorDefault, 0);
    require_noerr_action(SecCMSCreateSignedData(self_signed_identity, enveloped_data,
    parameters, simple_attr, signed_request), out, CFReleaseNull(signed_request));


out:
    CFReleaseSafe(simple_attr);
    CFReleaseSafe(msgtype_oid_data);
    CFReleaseSafe(msgtype_value_data);
    CFReleaseSafe(self_signed_identity);
    CFReleaseSafe(enveloped_data);
    CFReleaseSafe(csr);
    CFReleaseNull(realPublicKey);
    CFReleaseSafe(recipientKey);
    CFReleaseNull(filteredRecipients);
    return signed_request;
}

CFDataRef
SecSCEPCertifyRequest(CFDataRef request, SecIdentityRef ca_identity, CFDataRef serialno, bool pend_request) {
    return SecSCEPCertifyRequestWithAlgorithms(request, ca_identity, serialno, pend_request, NULL, NULL);
}

static SecCertificateRef copySignerCert(SecTrustRef trust) {
    CFArrayRef chain = SecTrustCopyCertificateChain(trust);
    if (!chain) {
        return NULL;
    }
    SecCertificateRef leaf = (SecCertificateRef)CFRetainSafe(CFArrayGetValueAtIndex(chain, 0));
    CFReleaseNull(chain);
    return leaf;
}

CFDataRef
SecSCEPCertifyRequestWithAlgorithms(CFDataRef request, SecIdentityRef ca_identity, CFDataRef serialno, bool pend_request,
                                    CFStringRef hashingAlgorithm, CFStringRef encryptionAlgorithm)
{
    CFDictionaryRef simple_attr = NULL;
    SecCertificateRef ca_certificate = NULL;
    SecKeyRef ca_public_key = NULL;
    SecCertificateRef cert = NULL;
    SecPolicyRef policy = NULL;
    CFDataRef cert_pkcs7 = NULL;
    CFMutableDataRef cert_msg = NULL;
    CFMutableDataRef signed_reply = NULL;
    SecTrustRef trust = NULL;
    CFDataRef signed_content = NULL;
    CFDictionaryRef signed_attributes = NULL;
    SecCertificateRef signer_cert = NULL;
    CFDataRef transid_oid_data = NULL, senderNonce_oid_data = NULL, transid_value = NULL;
    CFDataRef subject = NULL, extensions = NULL, senderNonce_value = NULL;
    CFStringRef challenge = NULL;
    SecKeyRef tbsPublicKey = NULL;
    CFMutableDataRef encrypted_content = NULL;
    SecCertificateRef recipient = NULL;
    CFMutableDictionaryRef parameters = NULL;
    
    require_noerr(SecIdentityCopyCertificate(ca_identity, &ca_certificate), out);
    ca_public_key = SecCertificateCopyKey(ca_certificate);

    /* unwrap outer layer: */
    policy = SecPolicyCreateBasicX509();

    require_noerr(SecCMSVerifyCopyDataAndAttributes(request, NULL, 
        policy, &trust, &signed_content, &signed_attributes), out);
    /* remember signer: is signer certified by us, then re-certify, no challenge needed */
    SecTrustResultType result;
    require_noerr(SecTrustEvaluate(trust, &result), out);
    require (signer_cert = copySignerCert(trust), out);
    bool recertify = !SecCertificateIsSignedBy(signer_cert, ca_public_key);
        
    /* msgType should be certreq msg */
    require(scep_attr_has_val(signed_attributes, messageType, PKCSReq), out);

    /* remember transaction id just for reuse */
    require(transid_oid_data = scep_oid(transId), out);
    require(transid_value = 
        dictionary_array_value_1(signed_attributes, transid_oid_data), out);
    
    /* senderNonce becomes recipientNonce */
    require(senderNonce_oid_data = scep_oid(senderNonce), out);
    require(senderNonce_value =
        dictionary_array_value_1(signed_attributes, senderNonce_oid_data), out);

    /* decrypt the request */
    encrypted_content = CFDataCreateMutable(kCFAllocatorDefault, 0);
    require_noerr(SecCMSDecryptEnvelopedData(signed_content, encrypted_content, &recipient), out);
    require(recipient, out);
    
    /* verify CSR */
    require(SecVerifyCertificateRequest(encrypted_content, &tbsPublicKey, &challenge, &subject, &extensions), out);
    CFReleaseNull(encrypted_content);

    /* @@@
    // alternatively send a pending message
    // pkistatus {{id-attributes pkiStatus(3)} "FAILURE"} 
    // failInfo {{id-attributes failInfo(4)} "the reason to reject"} 
    */

    /* verify challenge - this would need to be a callout that can determine
       the challenge appropriate for the subject */
    if (!recertify)
        require( challenge && (CFStringGetTypeID() == CFGetTypeID(challenge)) &&
            CFEqual(CFSTR("magic"), challenge), out);

	require(cert_msg = CFDataCreateMutable(kCFAllocatorDefault, 0), out);

	if (!pend_request) {
        /* We can't yet support EC recipients for SCEP, so reject now. */
        require (SecKeyGetAlgorithmId(tbsPublicKey) == kSecRSAAlgorithmID, out);

		/* sign cert */
		cert = SecIdentitySignCertificateWithAlgorithm(ca_identity, serialno,
			tbsPublicKey, subject, extensions, hashingAlgorithm);

		/* degenerate cms with cert */
		require (cert_pkcs7 = SecCMSCreateCertificatesOnlyMessage(cert), out);
		CFReleaseNull(cert);

		/* envelope for client */
        CFDictionaryRef encryption_params = NULL;
        if (encryptionAlgorithm) {
            encryption_params = CFDictionaryCreate(NULL, (const void **)&kSecCMSBulkEncryptionAlgorithm,
                                                   (const void **)&encryptionAlgorithm, 1,
                                                   &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);
        }
		require_noerr(SecCMSCreateEnvelopedData(signer_cert, encryption_params, cert_pkcs7, cert_msg), out);
		CFReleaseNull(cert_pkcs7);
        CFReleaseNull(encryption_params);
	}

	CFDataRef pki_status_oid = scep_oid(pkiStatus);
	CFDataRef pki_status_value = pend_request ? scep_result(PKIStatusPENDING) : scep_result(PKIStatusSUCCESS);
	CFDataRef message_type_oid = scep_oid(messageType), message_type_value = scep_result(CertRep);
	const void *oid[] = { transid_oid_data, pki_status_oid, message_type_oid };
	const void *value[] = { transid_value, pki_status_value, message_type_value };
	simple_attr = CFDictionaryCreate(kCFAllocatorDefault, oid, value, array_size(oid),
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFReleaseSafe(pki_status_oid); CFReleaseSafe(pki_status_value);
	CFReleaseSafe(message_type_oid); CFReleaseSafe(message_type_value);

	/* sign with ra/ca cert and add attributes */
	signed_reply = CFDataCreateMutable(kCFAllocatorDefault, 0);

    parameters = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(parameters, kSecCMSCertChainMode, kSecCMSCertChainModeNone);
    if (hashingAlgorithm) {
        CFDictionaryAddValue(parameters, kSecCMSSignHashAlgorithm, hashingAlgorithm);
    }
    require_noerr_action(SecCMSCreateSignedData(ca_identity, cert_msg, parameters, simple_attr, signed_reply), out, CFReleaseNull(signed_reply));

out:
    CFReleaseSafe(ca_certificate);
    CFReleaseSafe(ca_public_key);
    CFReleaseSafe(cert);
    CFReleaseSafe(cert_pkcs7);
    CFReleaseSafe(cert_msg);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(signed_content);
    CFReleaseSafe(signed_attributes);
    CFReleaseSafe(transid_oid_data);
    CFReleaseSafe(senderNonce_oid_data);
    CFReleaseSafe(subject);
    CFReleaseSafe(extensions);
    CFReleaseSafe(challenge);
    CFReleaseSafe(tbsPublicKey);
    CFReleaseSafe(encrypted_content);
    CFReleaseSafe(simple_attr);
    CFReleaseSafe(recipient);
    CFReleaseSafe(parameters);
    CFReleaseSafe(signer_cert);
    
    return signed_reply;
}

bool
SecSCEPVerifyGetCertInitial(CFDataRef request, SecIdentityRef ca_identity)
{
    SecCertificateRef ca_certificate = NULL;
    SecKeyRef ca_public_key = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDataRef signed_content = NULL;
    CFDictionaryRef signed_attributes = NULL;
    CFDataRef transid_oid_data = NULL, senderNonce_oid_data = NULL, transid_value = NULL, senderNonce_value = NULL;
    CFMutableDataRef encrypted_content = NULL;
    SecCertificateRef recipient = NULL;
    bool status = false;

    require_noerr(SecIdentityCopyCertificate(ca_identity, &ca_certificate), out);
    ca_public_key = SecCertificateCopyKey(ca_certificate);

    /* unwrap outer layer: */
    policy = SecPolicyCreateBasicX509();

    require_noerr(SecCMSVerifyCopyDataAndAttributes(request, NULL,
        policy, &trust, &signed_content, &signed_attributes), out);

    /* msgType should be certreq msg */
    require(scep_attr_has_val(signed_attributes, messageType, GetCertInitial), out);

    /* remember transaction id just for reuse */
    require(transid_oid_data = scep_oid(transId), out);
    require(transid_value =
        dictionary_array_value_1(signed_attributes, transid_oid_data), out);

    /* senderNonce becomes recipientNonce */
    require(senderNonce_oid_data = scep_oid(senderNonce), out);
    require(senderNonce_value =
        dictionary_array_value_1(signed_attributes, senderNonce_oid_data), out);

    /* decrypt the request */
    encrypted_content = CFDataCreateMutable(kCFAllocatorDefault, 0);
    require_noerr(SecCMSDecryptEnvelopedData(signed_content, encrypted_content, &recipient), out);
    require(recipient, out);
    require(CFDataGetLength(encrypted_content) > 0, out);
    status = true;

out:
    CFReleaseSafe(ca_certificate);
    CFReleaseSafe(ca_public_key);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(signed_content);
    CFReleaseSafe(signed_attributes);
    CFReleaseSafe(transid_oid_data);
    CFReleaseSafe(senderNonce_oid_data);
    CFReleaseSafe(encrypted_content);
    CFReleaseSafe(recipient);

    return status;
}

static CFStringRef
copy_signed_attr_printable_string_value(CFDictionaryRef signed_attributes, scep_attr_t attr)
{
	CFStringRef printable_string = NULL;
	CFDataRef key_oid = NULL;

	key_oid = scep_oid(attr);
	require(key_oid, out);

	CFArrayRef values = (CFArrayRef)CFDictionaryGetValue(signed_attributes, key_oid);
	require_quiet(values && (CFGetTypeID(values) == CFArrayGetTypeID())
			&& (CFArrayGetCount(values) == 1), out);
	CFDataRef value = CFArrayGetValueAtIndex(values, 0);
	const uint8_t *bytes = CFDataGetBytePtr(value);
	size_t length = CFDataGetLength(value);
	require(length >= 2, out);
	require(bytes[0] == 0x13, out);
	/* no scep responses defined that are longer */
	require(!(bytes[1] & 0x80) && (bytes[1] == length-2), out);
	printable_string = CFStringCreateWithBytes(kCFAllocatorDefault,
		bytes + 2, length - 2, kCFStringEncodingASCII, false);
out:
	CFReleaseSafe(key_oid);

	return printable_string;
}

CFArrayRef
SecSCEPVerifyReply(CFDataRef request, CFDataRef reply, CFTypeRef ca_certificates,
    CFErrorRef *server_error)
{
    SecKeyRef ca_public_key = NULL;
    SecPolicyRef policy = NULL;
    CFDataRef cert_msg = NULL;    
    CFMutableDataRef enc_cert_msg = NULL;
    SecTrustRef trust = NULL;
    CFDataRef signed_content = NULL;
    CFDictionaryRef signed_attributes = NULL;
    CFDictionaryRef attributes = NULL;
    SecCertificateRef signer_cert = NULL;

    CFMutableDataRef encrypted_content = NULL;
    SecCertificateRef recipient = NULL;
    CFArrayRef certificates = NULL;

    SecCertificateRef reply_signer = NULL;
    
    CFStringRef msg_type = NULL;
    CFStringRef pki_status = NULL;

    if (CFGetTypeID(ca_certificates) == SecCertificateGetTypeID()) {
        reply_signer = (SecCertificateRef)ca_certificates;
    } else if (CFGetTypeID(ca_certificates) == CFArrayGetTypeID()) {
        CFIndex reply_signer_count = CFArrayGetCount(ca_certificates);
        if (reply_signer_count > 1) {
            /* get the signer cert */
            reply_signer = (SecCertificateRef)CFArrayGetValueAtIndex(ca_certificates, 1);
        } else if (reply_signer_count == 1) {
            /* if there is at least one we'll assume it's sign+encrypt */
            reply_signer = (SecCertificateRef)CFArrayGetValueAtIndex(ca_certificates, 0);
        }
    }
    require(reply_signer, out);

    /* unwrap outer layer */
    policy = SecPolicyCreateBasicX509();
    CFArrayRef additional_certificates = CFArrayCreate(kCFAllocatorDefault, (const void **)&reply_signer, 1, &kCFTypeArrayCallBacks);
    require_noerr(SecCMSVerifySignedData(reply, NULL,
        policy, &trust, additional_certificates, &signed_content, &attributes), out);
    CFReleaseSafe(additional_certificates);
    if (attributes)
        signed_attributes = CFDictionaryGetValue(attributes, kSecCMSSignedAttributes);

    /* response should be signed by ra */
    SecTrustResultType result;
    require_noerr(SecTrustEvaluate(trust, &result), out);
    require(signer_cert = copySignerCert(trust), out);
    require(CFEqual(reply_signer, signer_cert), out);

    /* msgType should be certreq msg */
    require(signed_attributes, out);
    msg_type = copy_signed_attr_printable_string_value(signed_attributes, messageType);
    pki_status = copy_signed_attr_printable_string_value(signed_attributes, pkiStatus);

    if (msg_type || pki_status) {
        require(msg_type && CFEqual(msg_type, CFSTR("3")), out);

        require(pki_status, out);
        if (CFEqual(pki_status, CFSTR("2"))) {
            goto out; // FAILURE, the end (return NULL)
        } else if (CFEqual(pki_status, CFSTR("3"))) {
            CFDataRef transid_oid_data = NULL, transid_value = NULL;
            CFDictionaryRef err_dict = NULL;
            require(transid_oid_data = scep_oid(transId), inner_out);
            require(transid_value = dictionary_array_value_1(signed_attributes, transid_oid_data), inner_out);
            err_dict = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&transid_oid_data, (const void **)&transid_value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if (server_error)
                *server_error = CFErrorCreate(kCFAllocatorDefault, CFSTR("PENDING"), 3, err_dict);
        inner_out:
            CFReleaseSafe(err_dict);
            CFReleaseSafe(transid_oid_data);
            goto out;
        }
        require(CFEqual(pki_status, CFSTR("0")), out);
    }

    // can we decode the request?
    encrypted_content = CFDataCreateMutable(kCFAllocatorDefault, 0);
    require_noerr(SecCMSDecryptEnvelopedData(signed_content, encrypted_content, &recipient), out);
    require(recipient, out);
    // verify recipient belongs with our private key

    // verify CSR:
    require(certificates = SecCMSCertificatesOnlyMessageCopyCertificates(encrypted_content), out);

    // recipient is either our temporary self-signed cert or the old cert we just used
    // to recertify.  if we have new certificates and have stored them successfully we
    // can now get rid of the cert.
    /* XXX/cs
       This should move outside of thise function when we force a signer
       to be passed in */
#if !TARGET_OS_OSX // See rdar://81992896 (SecItemDelete in SecSCEPVerifyReply() doesn't do anything on macOS)
    CFDictionaryRef cert_delete = CFDictionaryCreate(NULL,
        (const void **)&kSecValueRef, (const void **)&recipient, 1, NULL, NULL);
    require_noerr_action(SecItemDelete(cert_delete), out,
        CFReleaseSafe(cert_delete));
    CFReleaseSafe(cert_delete);
#endif

out:
    CFReleaseSafe(ca_public_key);
    CFReleaseSafe(cert_msg);    
    CFReleaseSafe(enc_cert_msg);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(signed_content);
    CFReleaseSafe(encrypted_content);
    CFReleaseSafe(recipient);
    CFReleaseSafe(msg_type);
    CFReleaseSafe(pki_status);
    CFReleaseSafe(attributes);
    CFReleaseSafe(signer_cert);
    
    return certificates;
}

static CFDataRef SecCertificateCopyMD5Digest(SecCertificateRef cert)
{
    uint8_t cert_hash[CC_MD5_DIGEST_LENGTH];
    size_t cert_data_len = SecCertificateGetLength(cert);
    const uint8_t *cert_data = SecCertificateGetBytePtr(cert);
    require(cert_data_len && cert_data, out);
    require(cert_data_len<UINT32_MAX, out);
    CC_MD5(cert_data, (CC_LONG)cert_data_len, cert_hash);
    return CFDataCreate(NULL, cert_hash, CC_MD5_DIGEST_LENGTH);

out:
    return NULL;
}

static CFDataRef SecCertificateCopySHA224Digest(SecCertificateRef cert)
{
    uint8_t cert_hash[CC_SHA224_DIGEST_LENGTH];
    size_t cert_data_len = SecCertificateGetLength(cert);
    const uint8_t *cert_data = SecCertificateGetBytePtr(cert);
    require(cert_data_len && cert_data, out);
    require(cert_data_len<UINT32_MAX, out);
    CC_SHA224(cert_data, (CC_LONG)cert_data_len, cert_hash);
    return CFDataCreate(NULL, cert_hash, CC_SHA224_DIGEST_LENGTH);

out:
    return NULL;
}

static CFDataRef SecCertificateCopySHA384Digest(SecCertificateRef cert)
{
    uint8_t cert_hash[CC_SHA384_DIGEST_LENGTH];
    size_t cert_data_len = SecCertificateGetLength(cert);
    const uint8_t *cert_data = SecCertificateGetBytePtr(cert);
    require(cert_data_len && cert_data, out);
    require(cert_data_len<UINT32_MAX, out);
    CC_SHA384(cert_data, (CC_LONG)cert_data_len, cert_hash);
    return CFDataCreate(NULL, cert_hash, CC_SHA384_DIGEST_LENGTH);

out:
    return NULL;
}

static CFDataRef SecCertificateCopySHA512Digest(SecCertificateRef cert)
{
    uint8_t cert_hash[CC_SHA512_DIGEST_LENGTH];
    size_t cert_data_len = SecCertificateGetLength(cert);
    const uint8_t *cert_data = SecCertificateGetBytePtr(cert);
    require(cert_data_len && cert_data, out);
    require(cert_data_len<UINT32_MAX, out);
    CC_SHA512(cert_data, (CC_LONG)cert_data_len, cert_hash);
    return CFDataCreate(NULL, cert_hash, CC_SHA512_DIGEST_LENGTH);

out:
    return NULL;
}

static OSStatus scep_find_ca_cert(CFArrayRef certs, CFDataRef ca_fingerprint, SecCertificateRef CF_RETURNS_RETAINED *_ca_certificate)
{
    CFIndex fingerprint_len = CFDataGetLength(ca_fingerprint);
    OSStatus result = errSecNotTrusted;
    // Loop through certs to find one that matches the CA fingerprint
    for (CFIndex j = 0; j < CFArrayGetCount(certs); j++) {
        SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certs, j);
        CFDataRef cert_fingerprint = NULL;
        switch (fingerprint_len) {
            case CC_MD5_DIGEST_LENGTH:
                secinfo("scep", "MD5 fingerprint digest");
                cert_fingerprint = SecCertificateCopyMD5Digest(cert);
                break;
            case CC_SHA1_DIGEST_LENGTH:
                secinfo("scep", "SHA1 fingerprint digest");
                cert_fingerprint = CFRetainSafe(SecCertificateGetSHA1Digest(cert));
                break;
            case CC_SHA224_DIGEST_LENGTH:
                secinfo("scep", "SHA224 fingerprint digest");
                cert_fingerprint = SecCertificateCopySHA224Digest(cert);
                break;
            case CC_SHA256_DIGEST_LENGTH:
                secinfo("scep", "SHA256 fingerprint digest");
                cert_fingerprint = SecCertificateCopySHA256Digest(cert);
                break;
            case CC_SHA384_DIGEST_LENGTH:
                secinfo("scep", "SHA384 fingerprint digest");
                cert_fingerprint = SecCertificateCopySHA384Digest(cert);
                break;
            case CC_SHA512_DIGEST_LENGTH:
                secinfo("scep", "SHA512 fingerprint digest");
                cert_fingerprint = SecCertificateCopySHA512Digest(cert);
                break;
            default:
                secerror("SCEP failed to find algorithm to match CA fingerprint length: %ld", (long)fingerprint_len);
                return errSecInvalidDigestAlgorithm;
        }
        if (CFEqualSafe(cert_fingerprint, ca_fingerprint)) {
            if (_ca_certificate) {
                *_ca_certificate = CFRetainSafe(cert);
            }
            CFReleaseNull(cert_fingerprint);
            result = errSecSuccess;
            break;
        }
        CFReleaseNull(cert_fingerprint);
    }
    return result;
}

static OSStatus scep_find_ra_candidates(CFArrayRef certs, CFMutableArrayRef candidate_ra_certs, CFMutableArrayRef candidate_ra_signing_certs, CFMutableArrayRef candidate_ra_encryption_certs)
{
    if (!candidate_ra_certs || !candidate_ra_signing_certs || !candidate_ra_encryption_certs) {
        return errSecMemoryError;
    }
    for (CFIndex j = 0; j < CFArrayGetCount(certs); j++) {
        SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certs, j);
        SecKeyUsage key_usage = SecCertificateGetKeyUsage(cert);
        bool can_sign = (key_usage & kSecKeyUsageDigitalSignature);
        bool can_enc = (key_usage & kSecKeyUsageKeyEncipherment);
        if (can_sign && can_enc) {
            CFArrayAppendValue(candidate_ra_certs, cert);
        } else if (!can_sign && can_enc) {
            CFArrayAppendValue(candidate_ra_encryption_certs, cert);
        } else if (!can_enc && can_sign) {
            CFArrayAppendValue(candidate_ra_signing_certs, cert);
        }
    }

    // Fail if we have no candidate RA certs based on key usage
    if (CFArrayGetCount(candidate_ra_certs) == 0 &&
        (CFArrayGetCount(candidate_ra_signing_certs) == 0 || CFArrayGetCount(candidate_ra_encryption_certs) == 0)) {
        return errSecKeyUsageIncorrect;
    }

    return errSecSuccess;
}

static CF_RETURNS_RETAINED CFArrayRef scep_find_ra_chain(CFArrayRef candidates, CFArrayRef certs, SecCertificateRef ca_certificate)
{
    for (CFIndex j = 0; j < CFArrayGetCount(candidates); j++) {
        CFMutableArrayRef certsForTrust = CFArrayCreateMutable(NULL, CFArrayGetCount(certs) + 1, &kCFTypeArrayCallBacks);
        SecCertificateRef leaf = (SecCertificateRef)CFArrayGetValueAtIndex(candidates, j);
        CFArrayAppendValue(certsForTrust, leaf); // set candidate leaf first
        CFArrayAppendAll(certsForTrust, certs); // then all the other certs (SecTrust will ignore duplicates)
        SecTrustRef trust = NULL;
        if (errSecSuccess != SecTrustCreateWithCertificates(certsForTrust, NULL, &trust)) {
            secerror("SCEP failed to create trust for %@", leaf);
            CFReleaseNull(certsForTrust);
            continue;
        }
        CFReleaseNull(certsForTrust);
        if (ca_certificate) {
            /* If we found the CA certificate via the fingerprint, set it as the anchor cert */
            CFArrayRef anchors = CFArrayCreate(NULL, (const void**)&ca_certificate, 1, &kCFTypeArrayCallBacks);
            SecTrustSetAnchorCertificates(trust, anchors);
            CFReleaseNull(anchors);
        }
        CFArrayRef chain = SecTrustCopyCertificateChain(trust);
        if (!chain) {
            secnotice("scep", "failed to create chain %@", leaf);
            continue;
        } else if (ca_certificate && !CFEqualSafe(ca_certificate, CFArrayGetValueAtIndex(chain, CFArrayGetCount(chain) - 1))) {
            secnotice("scep", "failed to create chain from %@ to ca cert %@", leaf, ca_certificate);
            CFReleaseNull(chain);
            continue;
        }
        return chain;
    }
    return NULL;
}

OSStatus SecSCEPValidateCACertMessage(CFArrayRef certs,
    CFDataRef ca_fingerprint,
    SecCertificateRef CF_RETURNS_RETAINED *ca_certificate,
    SecCertificateRef CF_RETURNS_RETAINED *ra_signing_certificate,
    SecCertificateRef CF_RETURNS_RETAINED *ra_encryption_certificate)
{
    SecCertificateRef _ca_certificate = NULL, _ra_signing_certificate = NULL,
        _ra_encryption_certificate = NULL, _ra_certificate = NULL;

    /* If we have a CA fingerprint, first we need to find that in the certs to anchor */
    if (ca_fingerprint) {
        OSStatus result = scep_find_ca_cert(certs, ca_fingerprint, &_ca_certificate);
        if (result != errSecSuccess) {
            secerror("SCEP failed to find certificate matching CA fingerprint: %@", ca_fingerprint);
            return result;
        }
    }

    /* Find candidate RA certificates */
    CFMutableArrayRef candidate_ra_certs = CFArrayCreateMutable(NULL, CFArrayGetCount(certs), &kCFTypeArrayCallBacks);
    CFMutableArrayRef candidate_ra_signing_certs = CFArrayCreateMutable(NULL, CFArrayGetCount(certs), &kCFTypeArrayCallBacks);
    CFMutableArrayRef candidate_ra_encryption_certs = CFArrayCreateMutable(NULL, CFArrayGetCount(certs), &kCFTypeArrayCallBacks);
    OSStatus result = scep_find_ra_candidates(certs, candidate_ra_certs, candidate_ra_signing_certs, candidate_ra_encryption_certs);
    if (result != errSecSuccess) {
        secerror("SCEP failed to find candidate RA certificates");
        CFReleaseNull(_ca_certificate);
        CFReleaseNull(candidate_ra_certs);
        CFReleaseNull(candidate_ra_signing_certs);
        CFReleaseNull(candidate_ra_encryption_certs);
        return result;
    }

    /* Verify the candidate RA certificates chain to the CA cert, preferring different RA encryption/signing certs */
    result = errSecInternal;
    CFArrayRef ra_signing_chain = scep_find_ra_chain(candidate_ra_signing_certs, certs, _ca_certificate);
    CFArrayRef ra_encryption_chain = scep_find_ra_chain(candidate_ra_encryption_certs, certs, _ca_certificate);
    if (ra_signing_chain && ra_encryption_chain) {
        _ra_signing_certificate = (SecCertificateRef)CFRetainSafe(CFArrayGetValueAtIndex(ra_signing_chain, 0));
        _ra_encryption_certificate = (SecCertificateRef)CFRetainSafe(CFArrayGetValueAtIndex(ra_encryption_chain, 0));
        result = errSecSuccess;
        if (!_ca_certificate) {
            /* no CA fingerprint specified, so need to find CA cert using chains built */
            SecCertificateRef _signing_ca_cert = (SecCertificateRef)CFArrayGetValueAtIndex(ra_signing_chain, CFArrayGetCount(ra_signing_chain) - 1);
            SecCertificateRef _encryption_ca_cert = (SecCertificateRef)CFArrayGetValueAtIndex(ra_encryption_chain, CFArrayGetCount(ra_encryption_chain) - 1);
            if (CFEqualSafe(_signing_ca_cert, _encryption_ca_cert)) {
                _ca_certificate = CFRetainSafe(_signing_ca_cert);
            } else {
                secnotice("scep", "signing/encryption CAs do not match");
                result = errSecNotTrusted;
            }
        }
    }

    if (result == errSecSuccess) {
        /* return certs */
        if (ca_certificate) { *ca_certificate = CFRetainSafe(_ca_certificate); }
        if (ra_signing_certificate) { *ra_signing_certificate = CFRetainSafe(_ra_signing_certificate); }
        if (ra_encryption_certificate) { *ra_encryption_certificate = CFRetainSafe(_ra_encryption_certificate); }
    } else {
        secinfo("scep", "SCEP did not find different RA certificates for signing/encryption; looking for one cert");
        CFArrayRef ra_chain = scep_find_ra_chain(candidate_ra_certs, certs, _ca_certificate);
        if (ra_chain) {
            /* One RA cert for both signing and encryption */
            if (!_ca_certificate) {
                /* No CA fingerprint specified so derive from chain */
                _ca_certificate = (SecCertificateRef)CFRetainSafe(CFArrayGetValueAtIndex(ra_chain, CFArrayGetCount(ra_chain) - 1));
            }
            _ra_certificate = (SecCertificateRef)CFRetainSafe(CFArrayGetValueAtIndex(ra_chain, 0));
            CFReleaseNull(ra_chain);
            result = errSecSuccess;

            if (ca_certificate) { *ca_certificate = CFRetainSafe(_ca_certificate); }
            if (ra_signing_certificate) { *ra_signing_certificate = CFRetainSafe(_ra_certificate); }
        }
    }

    CFReleaseNull(ra_signing_chain);
    CFReleaseNull(ra_encryption_chain);
    CFReleaseNull(candidate_ra_certs);
    CFReleaseNull(candidate_ra_signing_certs);
    CFReleaseNull(candidate_ra_encryption_certs);
    CFReleaseNull(_ca_certificate);
    CFReleaseNull(_ra_certificate);
    CFReleaseNull(_ra_signing_certificate);
    CFReleaseNull(_ra_encryption_certificate);
    return result;
}


/*!
 @function SecSCEPGetCertInitial
 @abstract generate a scep cert initial request, to be presented to
 a scep server, in case the first request timed out
 */

// XXX/cs pass CA/RA certificates as a CFTypeRef: one or more certificates for ca_certificate and recipient

CF_RETURNS_RETAINED CFDataRef
SecSCEPGetCertInitial(SecCertificateRef ca_certificate, CFArrayRef subject, CFDictionaryRef parameters,
					  CFDictionaryRef signed_attrs, SecIdentityRef signer, CFTypeRef recipient)
{
    CFMutableDataRef signed_request = NULL;
    CFMutableDictionaryRef simple_attr = NULL;
    CFDataRef pki_message_contents = NULL;
    CFMutableDataRef enveloped_data = NULL;
    CFDataRef msgtype_value_data = NULL;
    CFDataRef msgtype_oid_data = NULL;
    CFTypeRef filteredRecipients = NULL;

    require(signed_attrs, out);
    require(pki_message_contents = SecGenerateCertificateRequestSubject(ca_certificate, subject), out);
    require(enveloped_data = CFDataCreateMutable(kCFAllocatorDefault, 0), out);
    filteredRecipients = filterRecipients(recipient);
    require_noerr(SecCMSCreateEnvelopedData(filteredRecipients, parameters, pki_message_contents, enveloped_data), out);

    /* remember transaction id just for reuse */
    simple_attr =  CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 3, signed_attrs);

    /* message type: GetCertInitial (20) */
    require(msgtype_oid_data = scep_oid(messageType), out);
    require(msgtype_value_data = printable_string_data(sizeof(GetCertInitial) - 1, GetCertInitial), out);
    CFDictionarySetValue(simple_attr, msgtype_oid_data, msgtype_value_data);
    CFReleaseNull(msgtype_oid_data);
    CFReleaseNull(msgtype_value_data);

    /* random sender nonce, to be verified against recipient nonce in reply */
	generate_sender_nonce(simple_attr);
    signed_request = CFDataCreateMutable(kCFAllocatorDefault, 0);
    require_noerr_action(SecCMSCreateSignedData(signer, enveloped_data,
	parameters, simple_attr, signed_request), out, CFReleaseNull(signed_request));

out:
	CFReleaseSafe(simple_attr);
	CFReleaseSafe(pki_message_contents);
	CFReleaseSafe(enveloped_data);
    CFReleaseSafe(msgtype_oid_data);
    CFReleaseSafe(msgtype_value_data);
    CFReleaseNull(filteredRecipients);
	return signed_request;
}


/*
     +----------------+-----------------+---------------------------+
     | Attribute      | Encoding        | Comment                   |
     +----------------+-----------------+---------------------------+
     | transactionID  | PrintableString | Decimal value as a string |
     | messageType    | PrintableString | Decimal value as a string |
     | pkiStatus      | PrintableString | Decimal value as a string |
     | failInfo       | PrintableString | Decimal value as a string |
     | senderNonce    | OctetString     |                           |
     | recipientNonce | OctetString     |                           |
     +----------------+-----------------+---------------------------+

4.2.1.  transactionID

   The transactionID is an attribute which uniquely identifies a
   transaction.  This attribute is required in all PKI messages.

   Because the enrollment transaction could be interrupted by various
   errors, including network connection errors or client reboot, the
   SCEP client generates a transaction identifier by calculating a hash
   on the public key value for which the enrollment is requested.  This
   retains the same transaction identifier throughout the enrollment
   transaction, even if the client has rebooted or timed out, and issues
   a new enrollment request for the same key pair.

   It also provides the way for the CA to uniquely identify a
   transaction in its database.  At the requester side, it generates a
   transaction identifier which is included in PKCSReq.  If the CA
   returns a response of PENDING, the requester will poll by
   periodically sending out GetCertInitial with the same transaction
   identifier until either a response other than PENDING is obtained, or
   the configured maximum time has elapsed.

   For non-enrollment message (for example GetCert and GetCRL), the
   transactionID should be a number unique to the client.


4.2.2.  messageType

   The messageType attribute specify the type of operation performed by
   the transaction.  This attribute is required in all PKI messages.
   Currently, the following message types are defined:

   o  PKCSReq (19) -- PKCS#10 [RFC2986] certificate request

   o  CertRep (3) -- Response to certificate or CRL request

   o  GetCertInitial (20) -- Certificate polling in manual enrollment

   o  GetCert (21) -- Retrieve a certificate

   o  GetCRL (22) -- Retrieve a CRL

4.2.3.  pkiStatus

   All response message will include transaction status information
   which is defined as pkiStatus attribute:

   o  SUCCESS (0) -- request granted

   o  FAILURE (2) -- request rejected.  This also requires a failInfo
      attribute to be present, as defined in section 4.2.4.

   o  PENDING (3) -- request pending for manual approval


4.2.4.  failInfo

   The failInfo attribute will contain one of the following failure
   reasons:

   o  badAlg (0) -- Unrecognized or unsupported algorithm ident

   o  badMessageCheck (1) -- integrity check failed

   o  badRequest (2) -- transaction not permitted or supported

   o  badTime (3) -- Message time field was not sufficiently close to
      the system time

   o  badCertId (4) -- No certificate could be identified matching the
      provided criteria

4.2.5.  senderNonce and responderNonce

   The attributes of senderNonce and recipientNonce are the 16 byte
   random numbers generated for each transaction to prevent the replay
   attack.

   When a requester sends a PKI message to the server, a senderNonce is
   included in the message.  After the server processes the request, it
   will send back the requester senderNonce as the recipientNonce and
   generates another nonce as the senderNonce in the response message.
   Because the proposed PKI protocol is a two-way communication
   protocol, it is clear that the nonce can only be used by the
   requester to prevent the replay.  The server has to employ extra
   state related information to prevent a replay attack.

*/
