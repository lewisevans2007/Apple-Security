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

#if OCTAGON

NS_ASSUME_NONNULL_BEGIN

@interface CKKSCurrentItemPointer : CKKSCKRecordHolder

@property CKKSProcessedState* state;
@property NSString* identifier;
@property NSString* currentItemUUID;

- (instancetype)initForIdentifier:(NSString*)identifier
                        contextID:(NSString*)contextID
                  currentItemUUID:(NSString*)currentItemUUID
                            state:(CKKSProcessedState*)state
                           zoneID:(CKRecordZoneID*)zoneID
                  encodedCKRecord:(NSData* _Nullable)encodedrecord;

+ (instancetype)fromDatabase:(NSString*)identifier
                   contextID:(NSString*)contextID
                       state:(CKKSProcessedState*)state
                      zoneID:(CKRecordZoneID*)zoneID
                       error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabase:(NSString*)identifier
                      contextID:(NSString*)contextID
                          state:(CKKSProcessedState*)state
                         zoneID:(CKRecordZoneID*)zoneID
                          error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSCurrentItemPointer*>*)remoteItemPointers:(CKRecordZoneID*)zoneID
                                              contextID:(NSString*)contextID
                                                  error:(NSError* __autoreleasing*)error;

+ (bool)deleteAll:(CKRecordZoneID*)zoneID
        contextID:(NSString*)contextID
            error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSCurrentItemPointer*>*)allInZone:(CKRecordZoneID*)zoneID
                                     contextID:(NSString*)contextID
                                         error:(NSError* __autoreleasing*)error;

+ (NSInteger)countByState:(CKKSItemState *)state
                contextID:(NSString*)contextID
                     zone:(CKRecordZoneID*)zoneID
                    error:(NSError * __autoreleasing *)error;

+ (BOOL)intransactionRecordChanged:(CKRecord*)record
                         contextID:(NSString*)contextID
                            resync:(BOOL)resync
                             error:(NSError**)error;

+ (BOOL)intransactionRecordDeleted:(CKRecordID*)recordID
                         contextID:(NSString*)contextID
                            resync:(BOOL)resync
                             error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END

#endif
