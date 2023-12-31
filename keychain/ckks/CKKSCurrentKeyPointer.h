/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ckks/CKKSResultOperation.h"

#if OCTAGON

NS_ASSUME_NONNULL_BEGIN

@interface CKKSCurrentKeyPointer : CKKSCKRecordHolder

@property CKKSKeyClass* keyclass;
@property NSString* currentKeyUUID;

- (instancetype)initForClass:(CKKSKeyClass*)keyclass
                   contextID:(NSString*)contextID
              currentKeyUUID:(NSString* _Nullable)currentKeyUUID
                      zoneID:(CKRecordZoneID*)zoneID
             encodedCKRecord:(NSData* _Nullable)encodedrecord;

+ (instancetype _Nullable)fromDatabase:(CKKSKeyClass*)keyclass
                             contextID:(NSString*)contextID
                                zoneID:(CKRecordZoneID*)zoneID
                                 error:(NSError* __autoreleasing*)error;

+ (instancetype _Nullable)tryFromDatabase:(CKKSKeyClass*)keyclass
                                contextID:(NSString*)contextID
                                   zoneID:(CKRecordZoneID*)zoneID
                                    error:(NSError* __autoreleasing*)error;

+ (instancetype _Nullable)forKeyClass:(CKKSKeyClass*)keyclass
                            contextID:(NSString*)contextID
                          withKeyUUID:(NSString*)keyUUID
                               zoneID:(CKRecordZoneID*)zoneID
                                error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSCurrentKeyPointer*>*)all:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (bool)deleteAll:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (BOOL)intransactionRecordChanged:(CKRecord*)record
                         contextID:(NSString*)contextID
                            resync:(BOOL)resync
                       flagHandler:(id<OctagonStateFlagHandler> _Nullable)flagHandler
                             error:(NSError**)error;

+ (BOOL)intransactionRecordDeleted:(CKRecordID*)recordID
                         contextID:(NSString*)contextID
                             error:(NSError**)error;
@end

@interface CKKSCurrentKeySet : NSObject
@property CKRecordZoneID* zoneID;
@property (readonly) NSString* contextID;
@property (nullable) NSError* error;
@property (nullable) CKKSKey* tlk;
@property (nullable) CKKSKey* classA;
@property (nullable) CKKSKey* classC;
@property (nullable) CKKSCurrentKeyPointer* currentTLKPointer;
@property (nullable) CKKSCurrentKeyPointer* currentClassAPointer;
@property (nullable) CKKSCurrentKeyPointer* currentClassCPointer;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithZoneID:(CKRecordZoneID*)zoneID
                     contextID:(NSString*)contextID;

// Set to true if this is a 'proposed' key set, i.e., not yet uploaded to CloudKit
@property BOOL proposed;

// This array (if present) holds any new TLKShares that should be uploaded
@property (nullable) NSArray<CKKSTLKShareRecord*>* pendingTLKShares;

+ (CKKSCurrentKeySet*)loadForZone:(CKRecordZoneID*)zoneID
                        contextID:(NSString*)contextID;

- (CKKSKeychainBackedKeySet* _Nullable)asKeychainBackedSet:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif
