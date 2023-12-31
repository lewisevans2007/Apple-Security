/*
 * Copyright (c) 2000,2002,2011-2014,2022 Apple Inc. All Rights Reserved.
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

/* 
 * ocspdClient.cpp - Client interface to OCSP helper daemon
 */

/* ****************************************************************************
 * IMPORTANT: The functionality provided by the OCSP helper daemon (ocspd)
 * has been subsumed by trustd as of macOS 12.0 (Monterey). SecTrust APIs
 * no longer rely on this library code or the ocspd daemon, and CDSA APIs
 * have been deprecated for the past decade. The client interface functions
 * in this file have been disabled as a first step toward removal.
 * ****************************************************************************
 */
 
#include "ocspdClient.h"
#include "ocspdTypes.h"
#include "ocspdDebug.h"
#include <Security/cssmapple.h>
#include <security_utilities/threading.h>
#include <security_utilities/mach++.h>
#include <security_utilities/unix++.h>
#include <security_ocspd/ocspd.h>			/* MIG interface */
#include <Security/SecBase.h>
class ocspdGlobals
{
public:
	ocspdGlobals();
	~ocspdGlobals();
    void resetServerPort();
    mach_port_t serverPort();
private:
	UnixPlusPlus::ForkMonitor mForkMonitor;
	MachPlusPlus::Port mServerPort;
	Mutex mLock;
};

ocspdGlobals::ocspdGlobals()
	: mServerPort(0)
{
	/* nothing here, the real work is done in serverPort() */
}

ocspdGlobals::~ocspdGlobals()
{
	/* I don't believe this should ever execute */
}

mach_port_t ocspdGlobals::serverPort()
{
#if OCSPD_ENABLED
	StLock<Mutex> _(mLock);
	
	// Guard against fork-without-exec. If we are the child of a fork
	// (that has not exec'ed), our apparent connection to SecurityServer
	// is just a mirage, and we better reset it.
	mach_port_t rtnPort = mServerPort.port();
	if (mForkMonitor()) {
		rtnPort = 0;
	}
	if(rtnPort != 0) {
		return rtnPort;
	}
	
	const char *serverName = NULL;
	#ifndef	NDEBUG
	serverName = getenv(OCSPD_BOOTSTRAP_ENV);
	#endif
	if(serverName == NULL) {
		serverName = (char*) OCSPD_BOOTSTRAP_NAME;
	}
	try {
		mServerPort = MachPlusPlus::Bootstrap().lookup2(serverName);
	}
	catch(...) {
		ocspdErrorLog("ocspdGlobals: error contacting server\n");
		throw;
	}
#endif
	return mServerPort;
}

void ocspdGlobals::resetServerPort()
{
#if OCSPD_ENABLED
    try {
        mServerPort.deallocate();
    } catch(...) {
    }
#endif
}


/* 
 * Perform network fetch of an OCSP response. Result is not verified in any 
 * way.
 */
CSSM_RETURN ocspdFetch(
	Allocator			&alloc,
	const CSSM_DATA		&ocspdReq,		// DER-encoded SecAsn1OCSPDRequests
	CSSM_DATA			&ocspdResp)		// DER-encoded kSecAsn1OCSPDReplies
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;
	unsigned char *rtnData = NULL;
	unsigned rtnLen = 0;
	
	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdFetch: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}

	krtn = ocsp_client_ocspdFetch(serverPort, ocspdReq.Data, (mach_msg_type_number_t)ocspdReq.Length,
		(void **)&rtnData, &rtnLen);
	if(krtn) {
		ocspdErrorLog("ocspdFetch: RPC returned %d\n", krtn);
		return CSSMERR_APPLETP_OCSP_UNAVAILABLE;
	}
	if((rtnData == NULL) || (rtnLen == 0)) {
		ocspdErrorLog("ocspdFetch: RPC returned NULL data\n");
		return CSSMERR_APPLETP_OCSP_UNAVAILABLE;
	}
	ocspdResp.Data = (uint8 *)alloc.malloc(rtnLen);
	ocspdResp.Length = rtnLen;
	memmove(ocspdResp.Data, rtnData, rtnLen);
	mig_deallocate((vm_address_t)rtnData, rtnLen);
	return CSSM_OK;
#else
	secerror("ocspdFetch is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/* 
 * Flush all responses associated with specifed CertID from cache. 
 */
CSSM_RETURN ocspdCacheFlush(
	const CSSM_DATA		&certID)
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;
	
	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdCacheFlush: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	krtn = ocsp_client_ocspdCacheFlush(serverPort, certID.Data, (mach_msg_type_number_t)certID.Length);
	if(krtn) {
		ocspdErrorLog("ocspdCacheFlush: RPC returned %d\n", krtn);
		return CSSMERR_APPLETP_OCSP_UNAVAILABLE;
	}
	return CSSM_OK;
#else
	secerror("ocspdCacheFlush is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/* 
 * Flush stale entries from cache. 
 */
CSSM_RETURN ocspdCacheFlushStale()
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;
	
	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdCacheFlush: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	krtn = ocsp_client_ocspdCacheFlushStale(serverPort);
	if(krtn) {
        if (krtn == MACH_SEND_INVALID_DEST)
            OcspdGlobals().resetServerPort();
		ocspdErrorLog("ocsp_client_ocspdCacheFlushStale: RPC returned %d\n", krtn);
		return (CSSM_RETURN)krtn;
	}
	return CSSM_OK;
#else
	secerror("ocspdCacheFlushStale is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/* 
 * fetch a certificate from the net. 
 */
CSSM_RETURN ocspdCertFetch(
	Allocator			&alloc,
	const CSSM_DATA		&certURL,
	CSSM_DATA			&certData)		// mallocd via alloc and RETURNED
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;
	unsigned char *rtnData = NULL;
	unsigned rtnLen = 0;
	
	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdCertFetch: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	
	krtn = ocsp_client_certFetch(serverPort, certURL.Data, (mach_msg_type_number_t)certURL.Length,
		(void **)&rtnData, &rtnLen);
	if(krtn) {
        if (krtn == MACH_SEND_INVALID_DEST)
            OcspdGlobals().resetServerPort();
		ocspdErrorLog("ocspdCertFetch: RPC returned %d\n", krtn);
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	
	if((rtnData == NULL) || (rtnLen == 0)) {
		ocspdErrorLog("ocspdCertFetch: RPC returned NULL data\n");
		return CSSMERR_APPLETP_CERT_NOT_FOUND_FROM_ISSUER;
	}
	certData.Data = (uint8 *)alloc.malloc(rtnLen);
	certData.Length = rtnLen;
	memmove(certData.Data, rtnData, rtnLen);
	mig_deallocate((vm_address_t)rtnData, rtnLen);
	return CSSM_OK;
#else
	secerror("ocspdCertFetch is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/*
 * Fetch a CRL from net with optional cache lookup and store.
 * verifyTime only used for cache lookup. 
 */
CSSM_RETURN ocspdCRLFetch(
	Allocator			&alloc,
	const CSSM_DATA		&crlURL,
	const CSSM_DATA		*crlIssuer,		// optional
	bool				cacheReadEnable,
	bool				cacheWriteEnable,
	CSSM_TIMESTRING 	verifyTime,
	CSSM_DATA			&crlData)		// mallocd via alloc and RETURNED
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;
	unsigned char *rtnData = NULL;
	unsigned rtnLen = 0;
	
	if(verifyTime == NULL) {
		ocspdErrorLog("ocspdCRLFetch: verifyTime NOT OPTIONAL\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdCRLFetch: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	
	krtn = ocsp_client_crlFetch(serverPort, crlURL.Data, (mach_msg_type_number_t)crlURL.Length,
		crlIssuer ? crlIssuer->Data : NULL, crlIssuer ? (mach_msg_type_number_t)crlIssuer->Length : 0,
		cacheReadEnable, cacheWriteEnable,
		verifyTime, (mach_msg_type_number_t)strlen(verifyTime),
		(void **)&rtnData, &rtnLen);
	if(krtn) {
        if (krtn == MACH_SEND_INVALID_DEST)
            OcspdGlobals().resetServerPort();
		ocspdErrorLog("ocspdCRLFetch: RPC returned %d\n", krtn);
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	
	if((rtnData == NULL) || (rtnLen == 0)) {
		ocspdErrorLog("ocspdCRLFetch: RPC returned NULL data\n");
		return CSSMERR_APPLETP_CRL_NOT_FOUND;
	}
	crlData.Data = (uint8 *)alloc.malloc(rtnLen);
	crlData.Length = rtnLen;
	memmove(crlData.Data, rtnData, rtnLen);
	mig_deallocate((vm_address_t)rtnData, rtnLen);
	return CSSM_OK;
#else
	secerror("ocspdCRLFetch is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}


/*
 * Get CRL status for given serial number and issuing entity
 */
CSSM_RETURN ocspdCRLStatus(
	const CSSM_DATA		&serialNumber,
	const CSSM_DATA		&issuers,
	const CSSM_DATA		*crlIssuer,		// optional if URL is supplied
	const CSSM_DATA		*crlURL)		// optional if issuer is supplied
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;

	if(!crlIssuer && !crlURL) {
		ocspdErrorLog("ocspdCRLStatus: either an issuer or URL is required\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	try {
		serverPort = OcspdGlobals().serverPort();
	}
	catch(...) {
		ocspdErrorLog("ocspdCRLStatus: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}

	krtn = ocsp_client_crlStatus(serverPort,
		serialNumber.Data, (mach_msg_type_number_t)serialNumber.Length,
		issuers.Data, (mach_msg_type_number_t)issuers.Length,
		crlIssuer ? crlIssuer->Data : NULL, crlIssuer ? (mach_msg_type_number_t)crlIssuer->Length : 0,
		crlURL ? crlURL->Data : NULL, crlURL ? (mach_msg_type_number_t)crlURL->Length : 0);
    if (krtn == MACH_SEND_INVALID_DEST) {
        OcspdGlobals().resetServerPort();
    }

    return krtn;
#else
    secerror("ocspdCRLStatus is disabled and should no longer be called.");
    return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/*
 * Refresh the CRL cache.
 */
CSSM_RETURN ocspdCRLRefresh(
	unsigned	staleDays,
	unsigned	expireOverlapSeconds,
	bool		purgeAll,
	bool		fullCryptoVerify)
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;
	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdCRLRefresh: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	
	krtn = ocsp_client_crlRefresh(serverPort, staleDays, expireOverlapSeconds,
		purgeAll, fullCryptoVerify);
	if(krtn) {
        if (krtn == MACH_SEND_INVALID_DEST)
            OcspdGlobals().resetServerPort();
		ocspdErrorLog("ocspdCRLRefresh: RPC returned %d\n", krtn);
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	
	return CSSM_OK;
#else
	secerror("ocspdCRLRefresh is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/*
 * Flush all CRLs obtained from specified URL from cache. Called by client when
 * *it* detects a bad CRL.
 */
CSSM_RETURN ocspdCRLFlush(
	const CSSM_DATA		&crlURL)
{
#if OCSPD_ENABLED
    mach_port_t serverPort = 0;
	kern_return_t krtn;

	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdCRLFlush: OCSPD server error\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	
	krtn = ocsp_client_crlFlush(serverPort, crlURL.Data, (mach_msg_type_number_t)crlURL.Length);
	if(krtn) {
        if (krtn == MACH_SEND_INVALID_DEST)
            OcspdGlobals().resetServerPort();
		ocspdErrorLog("ocspdCRLFlush: RPC returned %d\n", krtn);
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	return CSSM_OK;
#else
	secerror("ocspdCRLFlush is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/*
 * Obtain TrustSettings. 
 */
OSStatus ocspdTrustSettingsRead(
	Allocator				&alloc,
	SecTrustSettingsDomain 	domain,
	CSSM_DATA				&trustSettings)		// mallocd via alloc and RETURNED
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	kern_return_t krtn;
	unsigned char *rtnData = NULL;
	unsigned rtnLen = 0;
	OSStatus ortn;

	try {
		serverPort = OcspdGlobals().serverPort();
	} 
	catch(...) {
		ocspdErrorLog("ocspdTrustSettingsRead: OCSPD server error\n");
		return errSecInternalComponent;
	}
	
	krtn = ocsp_client_trustSettingsRead(serverPort, domain,
		(void **)&rtnData, &rtnLen, &ortn);
	if(krtn) {
        if (krtn == MACH_SEND_INVALID_DEST)
            OcspdGlobals().resetServerPort();
		ocspdErrorLog("ocspdTrustSettingsRead: RPC returned %d\n", krtn);
		return errSecNotAvailable;
	}
	if(ortn) {
		/* e.g., errSecNoUserTrustRecord */
		return ortn;
	}
	if((rtnData == NULL) || (rtnLen == 0)) {
		ocspdErrorLog("ocspdTrustSettingsRead: RPC returned NULL data\n");
		return errSecItemNotFound;
	}
	trustSettings.Data = (uint8 *)alloc.malloc(rtnLen);
	trustSettings.Length = rtnLen;
	memmove(trustSettings.Data, rtnData, rtnLen);
	mig_deallocate((vm_address_t)rtnData, rtnLen);
	return errSecSuccess;
#else
	secerror("ocspdTrustSettingsRead is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}

/*
 * Write TrustSettings to disk. Results in authentication dialog.
 */
OSStatus ocspdTrustSettingsWrite(
	SecTrustSettingsDomain 	domain,
	const CSSM_DATA			&authBlob,
	const CSSM_DATA			&trustSettings)
{
#if OCSPD_ENABLED
	mach_port_t serverPort = 0;
	mach_port_t clientPort = 0;
	kern_return_t krtn;
	OSStatus ortn;

	try {
		serverPort = OcspdGlobals().serverPort();
		clientPort = MachPlusPlus::Bootstrap();
	} 
	catch(...) {
		ocspdErrorLog("ocspdTrustSettingsWrite: OCSPD server error\n");
		return errSecInternalComponent;
	}

	krtn = ocsp_client_trustSettingsWrite(serverPort, clientPort, domain,
		authBlob.Data, (mach_msg_type_number_t)authBlob.Length,
		trustSettings.Data, (mach_msg_type_number_t)trustSettings.Length,
		&ortn);
	if(krtn) {
        if (krtn == MACH_SEND_INVALID_DEST)
            OcspdGlobals().resetServerPort();
		ocspdErrorLog("ocspdTrustSettingsWrite: RPC returned %d\n", krtn);
		return errSecInternalComponent;
	}
	return ortn;
#else
	secerror("ocspdTrustSettingsWrite is disabled and should no longer be called.");
	return CSSMERR_TP_INTERNAL_ERROR;
#endif
}
