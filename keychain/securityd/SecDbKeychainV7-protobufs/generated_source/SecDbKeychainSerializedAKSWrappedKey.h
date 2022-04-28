// This file was automatically generated by protocompiler
// DO NOT EDIT!
// Compiled from SecDbKeychainSerializedAKSWrappedKey.proto

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBCodable.h>

#ifdef __cplusplus
#define SECDBKEYCHAINSERIALIZEDAKSWRAPPEDKEY_FUNCTION extern "C"
#else
#define SECDBKEYCHAINSERIALIZEDAKSWRAPPEDKEY_FUNCTION extern
#endif

@interface SecDbKeychainSerializedAKSWrappedKey : PBCodable <NSCopying>
{
    NSData *_refKeyBlob;
    uint32_t _type;
    NSData *_wrappedKey;
}


@property (nonatomic, retain) NSData *wrappedKey;

@property (nonatomic, readonly) BOOL hasRefKeyBlob;
@property (nonatomic, retain) NSData *refKeyBlob;

@property (nonatomic) uint32_t type;

// Performs a shallow copy into other
- (void)copyTo:(SecDbKeychainSerializedAKSWrappedKey *)other;

// Performs a deep merge from other into self
// If set in other, singular values in self are replaced in self
// Singular composite values are recursively merged
// Repeated values from other are appended to repeated values in self
- (void)mergeFrom:(SecDbKeychainSerializedAKSWrappedKey *)other;

SECDBKEYCHAINSERIALIZEDAKSWRAPPEDKEY_FUNCTION BOOL SecDbKeychainSerializedAKSWrappedKeyReadFrom(__unsafe_unretained SecDbKeychainSerializedAKSWrappedKey *self, __unsafe_unretained PBDataReader *reader);

@end
