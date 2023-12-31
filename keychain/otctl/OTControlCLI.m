

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecItemPriv.h>
#import <Security/Security.h>
#import <xpc/xpc.h>

#include "utilities/SecCFWrappers.h"
#include "utilities/SecInternalReleasePriv.h"
#import "utilities/debugging.h"

#import "keychain/ot/ErrorUtils.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OT.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTControl.h"

#import "keychain/otctl/OTControlCLI.h"

#import "keychain/OctagonTrust/OctagonTrust.h"
#import <OctagonTrust/OTCustodianRecoveryKey.h>
#import <OctagonTrust/OTInheritanceKey.h>

#import <AuthKit/AKAppleIDAuthenticationController.h>
#import <AuthKit/AKAppleIDAuthenticationContext.h>
#import <AuthKit/AKAppleIDAuthenticationContext_Private.h>

#import <AppleFeatures/AppleFeatures.h>

static NSString * fetch_pet(NSString * appleID, NSString * dsid)
{
    if(!appleID && !dsid) {
        NSLog(@"Must provide either an AppleID or a DSID to fetch a PET");
        exit(1);
    }

    AKAppleIDAuthenticationContext* authContext = [[AKAppleIDAuthenticationContext alloc] init];
    authContext.username = appleID;

    authContext.authenticationType = AKAppleIDAuthenticationTypeSilent;
    authContext.isUsernameEditable = NO;

    __block NSString * pet = nil;

    dispatch_semaphore_t s = dispatch_semaphore_create(0);

    AKAppleIDAuthenticationController *authenticationController = [[AKAppleIDAuthenticationController alloc] init];
    [authenticationController authenticateWithContext:authContext
                                           completion:^(AKAuthenticationResults authenticationResults, NSError *error) {
        if(error) {
            NSLog(@"error fetching PET: %@", error);
            exit(1);
        }

        pet = authenticationResults[AKAuthenticationPasswordKey];
        dispatch_semaphore_signal(s);
    }];
    dispatch_semaphore_wait(s, DISPATCH_TIME_FOREVER);

    return pet;
}

// Mutual recursion to set up an object for jsonification
static NSDictionary* cleanDictionaryForJSON(NSDictionary* dict);

static id cleanObjectForJSON(id obj) {
    if(!obj) {
        return nil;
    }
    if([obj isKindOfClass:[NSError class]]) {
        NSError* obje = (NSError*) obj;
        NSMutableDictionary* newErrorDict = [@{@"code": @(obje.code), @"domain": obje.domain} mutableCopy];
        newErrorDict[@"userInfo"] = cleanDictionaryForJSON(obje.userInfo);
        return newErrorDict;
    } else if([NSJSONSerialization isValidJSONObject:obj]) {
        return obj;

    } else if ([obj isKindOfClass: [NSNumber class]]) {
        return obj;

    } else if([obj isKindOfClass: [NSData class]]) {
        NSData* dataObj = (NSData*)obj;
        return [dataObj base64EncodedStringWithOptions:0];

    } else if ([obj isKindOfClass: [NSDictionary class]]) {
        return cleanDictionaryForJSON((NSDictionary*) obj);

    } else if ([obj isKindOfClass: [NSArray class]]) {
        NSArray* arrayObj = (NSArray*)obj;
        NSMutableArray* cleanArray = [NSMutableArray arrayWithCapacity:arrayObj.count];

        for(id x in arrayObj) {
            [cleanArray addObject: cleanObjectForJSON(x)];
        }
        return cleanArray;

    } else {
        return [obj description];
    }
}

static NSDictionary* cleanDictionaryForJSON(NSDictionary* dict) {
    if(!dict) {
        return nil;
    }
    NSMutableDictionary* mutDict = [dict mutableCopy];
    for(id key in mutDict.allKeys) {
        id obj = mutDict[key];
        mutDict[key] = cleanObjectForJSON(obj);
    }
    return mutDict;
}

static void print_json(NSDictionary* dict)
{
    NSError* err;

    NSData* json = [NSJSONSerialization dataWithJSONObject:cleanDictionaryForJSON(dict)
                                                   options:(NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys)
                                                     error:&err];
    if(!json) {
        NSLog(@"error during JSONification: %@", err.localizedDescription);
    } else {
        printf("%s\n", [[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding] UTF8String]);
    }
}


@implementation OTControlCLI

- (instancetype)initWithOTControl:(OTControl*)control {
    if((self = [super init])) {
        _control = control;
    }

    return self;
}

- (int)startOctagonStateMachine:(OTControlArguments*)arguments {
#if OCTAGON
    __block int ret = 1;

    [self.control startOctagonStateMachine:arguments
                                     reply:^(NSError* _Nullable error) {
        if(error) {
            fprintf(stderr, "Error starting state machine: %s\n", [[error description] UTF8String]);
        } else {
            printf("state machine started.\n");
            ret = 0;
        }
    }];

    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)signIn:(OTControlArguments*)arguments {
#if OCTAGON
    __block int ret = 1;

    [self.control appleAccountSignedIn:arguments
                                 reply:^(NSError* _Nullable error) {
        if(error) {
            fprintf(stderr, "Error signing in: %s\n", [[error description] UTF8String]);
        } else {
            printf("Sign in complete.\n");
            ret = 0;
        }
    }];

    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)signOut:(OTControlArguments*)arguments {
#if OCTAGON
    __block int ret = 1;
    [self.control appleAccountSignedOut:arguments
                                  reply:^(NSError* _Nullable error) {
        if(error) {
            fprintf(stderr, "Error signing out: %s\n", [[error description] UTF8String]);
        } else {
            printf("Sign out complete.\n");
            ret = 0;
        }
    }];
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)depart:(OTControlArguments*)arguments
{
#if OCTAGON
    __block int ret = 1;

    [self.control leaveClique:arguments
                        reply:^(NSError* _Nullable error) {
        if(error) {
            fprintf(stderr, "Error departing clique: %s\n", [[error description] UTF8String]);
        } else {
            printf("Departing clique completed.\n");
            ret = 0;
        }
    }];
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)resetOctagon:(OTControlArguments*)arguments timeout:(NSTimeInterval)timeout {
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    do {
        retry = false;
        [self.control resetAndEstablish:arguments
                            resetReason:CuttlefishResetReasonUserInitiatedReset
                                  reply:^(NSError* _Nullable error) {
            if(error) {
                fprintf(stderr, "Error resetting: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("reset and establish:\n");
                ret = 0;
            }
        }];
    } while(retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}


- (int)resetProtectedData:(OTControlArguments*)arguments
                  appleID:(NSString *_Nullable)appleID
                     dsid:(NSString *_Nullable)dsid
{
#if OCTAGON
    __block int ret = 1;

    NSError* error = nil;
    OTConfigurationContext *data = [[OTConfigurationContext alloc] init];
    data.passwordEquivalentToken = fetch_pet(appleID, dsid);
    data.authenticationAppleID = appleID;
    data.altDSID = arguments.altDSID;
    data.context = arguments.contextID;
    data.containerName = arguments.containerName;

    OTClique* clique = [OTClique resetProtectedData:data error:&error];
    if(clique != nil && error == nil) {
        printf("resetProtectedData succeeded\n");
        ret = 0;
    } else {
        fprintf(stderr, "resetProtectedData failed: %s\n", [[error description] UTF8String]);
    }
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (void)printPeer:(NSDictionary*)peerInformation prefix:(NSString * _Nullable)prefix {
    NSString * peerID = peerInformation[@"peerID"];
    NSString * model = peerInformation[@"permanentInfo"][@"model_id"];
    NSNumber* epoch = peerInformation[@"permanentInfo"][@"epoch"];
    NSString * deviceName = peerInformation[@"stableInfo"][@"device_name"];
    NSString * serialNumber = peerInformation[@"stableInfo"][@"serial_number"];
    NSString * os = peerInformation[@"stableInfo"][@"os_version"];

    printf("%s%s hw:'%s' name:'%s' serial: '%s' os:'%s' epoch:%d\n",
           (prefix ? [prefix UTF8String] : ""),
           [peerID UTF8String],
           [model UTF8String],
           [deviceName UTF8String],
           [serialNumber UTF8String],
           [os UTF8String],
           [epoch intValue]);
}

- (void)printCRKWithPeer:(NSString*)peerID information:(NSDictionary*)crkInformation prefix:(NSString * _Nullable)prefix {
    NSString *uuid = crkInformation[@"uuid"];
    NSString *kind = crkInformation[@"kind"];

    printf("%s%s uuid: %s kind: %s\n",
           (prefix ? [prefix UTF8String] : ""),
           [peerID UTF8String],
           [uuid UTF8String],
           (kind ? [kind UTF8String] : "-"));
}

- (void)printPeers:(NSArray<NSString *>*)peerIDs
         egoPeerID:(NSString * _Nullable)egoPeerID
informationOnPeers:(NSDictionary<NSString *, NSDictionary*>*)informationOnPeers
 informationOnCRKs:(NSDictionary<NSString *, NSDictionary*>*)informationOnCRKs
{
    for(NSString * peerID in peerIDs) {
        NSDictionary* peerInformation = informationOnPeers[peerID];

        if (peerInformation != nil) {
            if([peerID isEqualToString:egoPeerID]) {
                [self printPeer:peerInformation prefix:@"    Self: "];
            } else {
                [self printPeer:peerInformation prefix:@"    Peer: "];
            }
        } else {
            NSDictionary* crkInformation = informationOnCRKs[peerID];
            if (crkInformation != nil) {
                [self printCRKWithPeer:peerID information:crkInformation prefix:@"    CRK: "];
            } else {
                printf("    Peer:  %s; further information missing\n", [peerID UTF8String]);
            }
        }
    }
}

#if OCTAGON
- (int)checkAndPrintEscrowRecords:(NSArray<OTEscrowRecord*>*)records error:(NSError*)error json:(bool)json
{
    int ret = 1;
    if(records != nil && error == nil) {
        if (!json) {
            printf("Successfully fetched %lu record%s.\n", (unsigned long)records.count, records.count == 1 ? "" : "s");
        }
        ret = 0;
        NSMutableArray<NSString *>* escrowRecords = [NSMutableArray array];
        for(OTEscrowRecord* record in records){
            SOSPeerInfoRef peer = SOSPeerInfoCreateFromData(kCFAllocatorDefault, NULL, (__bridge CFDataRef)record.escrowInformationMetadata.peerInfo);
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            [escrowRecords addObject:(__bridge NSString *)peerID];
        }
        if (json) {
            print_json(@{@"escrowRecords" : escrowRecords});
        } else {
            for(NSString* s in escrowRecords) {
                printf("fetched record id: %s\n", [s UTF8String]);
            }
        }
    } else {
        if (json) {
            print_json(@{@"error" : [error description]});
        } else {
            fprintf(stderr, "fetching escrow records failed: %s\n", [[error description] UTF8String]);
        }
    }
    return ret;
}
#endif

- (int)fetchEscrowRecords:(OTControlArguments*)arguments json:(bool)json
{
#if OCTAGON
    NSError* error = nil;
    NSArray<OTEscrowRecord*>* records = [OTClique fetchEscrowRecords:[arguments makeConfigurationContext] error:&error];
    return [self checkAndPrintEscrowRecords:records error:error json:json];
#else
    if (json) {
        print_json(@{@"unimplemented" : @YES});
    } else {
        fprintf(stderr, "Unimplemented.\n");
    }
    return 1;
#endif
}

- (int)fetchAllEscrowRecords:(OTControlArguments*)arguments json:(bool)json
{
#if OCTAGON
    NSError* error = nil;
    NSArray<OTEscrowRecord*>* records = [OTClique fetchAllEscrowRecords:[arguments makeConfigurationContext] error:&error];
    return [self checkAndPrintEscrowRecords:records error:error json:json];
#else
    if (json) {
        print_json(@{@"unimplemented" : @YES});
    } else {
        fprintf(stderr, "Unimplemented.\n");
    }
    return 1;
#endif
}

- (int)performEscrowRecovery:(OTControlArguments*)arguments
                    recordID:(NSString *)recordID
                     appleID:(NSString *)appleID
                      secret:(NSString *)secret
    overrideForAccountScript:(BOOL)overrideForAccountScript
         overrideEscrowCache:(BOOL)overrideEscrowCache
{
#if OCTAGON
    __block int ret = 1;

    NSError* error = nil;

    OTICDPRecordContext* cdpContext = [[OTICDPRecordContext alloc] init];
    cdpContext.cdpInfo = [[OTCDPRecoveryInformation alloc] init];
    cdpContext.cdpInfo.recoverySecret = secret;
    cdpContext.cdpInfo.containsIcdpData = true;
    cdpContext.cdpInfo.usesMultipleIcsc = true;
    cdpContext.authInfo = [[OTEscrowAuthenticationInformation alloc] init];
    cdpContext.authInfo.authenticationAppleid = appleID;
    cdpContext.authInfo.authenticationPassword = fetch_pet(appleID, nil);
    
    OTConfigurationContext* contextForFetch = [arguments makeConfigurationContext];
    contextForFetch.overrideEscrowCache = overrideEscrowCache;
    contextForFetch.overrideForSetupAccountScript = overrideForAccountScript;

    NSArray<OTEscrowRecord*>* escrowRecords = [OTClique fetchEscrowRecords:contextForFetch error:&error];
    if (escrowRecords == nil || error != nil) {
        fprintf(stderr, "Failed to fetch escrow records: %s\n", [[error description] UTF8String]);
        return 1;
    }
    OTEscrowRecord* record = nil;
    
    for (OTEscrowRecord* r in escrowRecords) {
        CFErrorRef* localError = NULL;
        SOSPeerInfoRef peer = SOSPeerInfoCreateFromData(kCFAllocatorDefault, localError, (__bridge CFDataRef)r.escrowInformationMetadata.peerInfo);
        CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
        
        if ([(__bridge NSString *)peerID isEqualToString:recordID]) {
            record = r;
            break;
        }
    }
    if (record == nil){
        fprintf(stderr, "Failed to find escrow record to restore.\n");
        return 1;
    }
    
    OTConfigurationContext* context = [arguments makeConfigurationContext];
    context.overrideEscrowCache = YES;
    
    OTClique* clique = [OTClique performEscrowRecovery:context cdpContext:cdpContext escrowRecord:record error:&error];
    if (clique != nil && error == nil) {
        printf("Successfully performed escrow recovery.\n");
        ret = 0;
    } else {
        fprintf(stderr, "Escrow recovery failed: %s\n", error.description.UTF8String);
    }
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)performSilentEscrowRecovery:(OTControlArguments*)arguments
                           appleID:(NSString *)appleID
                            secret:(NSString *)secret
{
#if OCTAGON
    __block int ret = 1;
    
    NSError* error = nil;

    OTICDPRecordContext* cdpContext = [[OTICDPRecordContext alloc] init];
    cdpContext.cdpInfo = [[OTCDPRecoveryInformation alloc] init];

    cdpContext.cdpInfo.recoverySecret = secret;
    cdpContext.cdpInfo.containsIcdpData = true;
    cdpContext.cdpInfo.silentRecoveryAttempt = true;
    cdpContext.cdpInfo.usesMultipleIcsc = true;

    cdpContext.authInfo = [[OTEscrowAuthenticationInformation alloc] init];
    cdpContext.authInfo.authenticationAppleid = appleID;
    cdpContext.authInfo.authenticationPassword = fetch_pet(appleID, nil);

    NSArray<OTEscrowRecord*>* records = [OTClique fetchEscrowRecords:[arguments makeConfigurationContext] error:&error];
    if (records == nil || error != nil) {
        fprintf(stderr, "Failed to fetch escrow records: %s.\n", error.description.UTF8String);
    } else {
        OTClique* clique = [OTClique performSilentEscrowRecovery:[arguments makeConfigurationContext] cdpContext:cdpContext allRecords:records error:&error];
        if (clique != nil && error == nil) {
            printf("Successfully performed escrow recovery.\n");
            ret = 0;
        } else {
            fprintf(stderr, "Escrow recovery failed: %s\n", error.description.UTF8String);
        }
    }
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)tlkRecoverability:(OTControlArguments*)arguments
{
#if OCTAGON
    __block int ret = 1;
    
    NSError* error = nil;

    OTClique *clique = [[OTClique alloc] initWithContextData:[arguments makeConfigurationContext]];
    if (clique == nil) {
        fprintf(stderr, "Failed to create clique\n");
        return ret;
    }
    NSArray<OTEscrowRecord*>* records = [OTClique fetchAllEscrowRecords:[arguments makeConfigurationContext] error:&error];
    if (records == nil || error != nil) {
        fprintf(stderr, "Failed to fetch escrow records: %s.\n", error.description.UTF8String);
        return ret;
    } else {
        for (OTEscrowRecord *record in records) {
            NSError* localError = nil;
            NSArray<NSString*>* views = [clique tlkRecoverabilityForEscrowRecord:record error:&localError];
            if (views && [views count] > 0 && localError == nil) {
                for (NSString *view in views) {
                    printf("%s has recoverable view: %s\n", record.recordId.UTF8String, [view UTF8String]);
                }
                ret = 0;
            } else {
                fprintf(stderr, "%s Failed TLK recoverability check: %s\n", record.recordId.UTF8String, localError.description.UTF8String);
            }
        }
    }
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)status:(OTControlArguments*)arguments json:(bool)json {
#if OCTAGON
    __block int ret = 1;

    [self.control status:arguments
                   reply:^(NSDictionary* result, NSError* _Nullable error) {
                       if(error) {
                           if(json) {
                               print_json(@{@"error" : [error description]});
                           } else {
                               fprintf(stderr, "Error fetching status: %s\n", [[error description] UTF8String]);
                           }
                       } else {
                           ret = 0;
                           if(json) {
                               print_json(result);
                           } else {
                               printf("Status for %s,%s:\n", [result[@"containerName"] UTF8String], [result[@"contextID"] UTF8String]);
                               printf("Active account: %s\n", [result[@"activeAccount"] UTF8String]);

                               printf("\n");
                               printf("State: %s\n", [[result[@"state"] description] UTF8String]);
                               printf("Flags: %s; Flags Pending: %s\n\n",
                                      ([result[@"stateFlags"] count] == 0u) ? "none" : [[result[@"stateFlags"] description] UTF8String],
                                      ([result[@"statePendingFlags"] count] == 0u) ? "none" : [[result[@"statePendingFlags"] description] UTF8String]);

                               NSDictionary* contextDump = result[@"contextDump"];

                               // Make it easy to find peer information
                               NSMutableDictionary<NSString *, NSDictionary*>* peers = [NSMutableDictionary dictionary];
                               NSMutableArray<NSString *>* allPeerIDs = [NSMutableArray array];
                               for(NSDictionary* peerInformation in contextDump[@"peers"]) {
                                   NSString * peerID = peerInformation[@"peerID"];
                                   if(peerID) {
                                       peers[peerID] = peerInformation;
                                       [allPeerIDs addObject:peerID];
                                   }
                               }

                               NSMutableDictionary<NSString *, NSDictionary*>* crks = [NSMutableDictionary dictionary];
                               for(NSDictionary* crkInformation in contextDump[@"custodian_recovery_keys"]) {
                                   NSString * peerID = crkInformation[@"peerID"];
                                   if (peerID) {
                                       crks[peerID] = crkInformation;
                                   }
                               }

                               NSDictionary* egoInformation = contextDump[@"self"];
                               NSString * egoPeerID = egoInformation[@"peerID"];
                               NSDictionary* egoDynamicInfo = egoInformation[@"dynamicInfo"];

                               if(egoPeerID) {
                                   NSMutableArray *otherPeers = [allPeerIDs mutableCopy];
                                   [self printPeer:egoInformation prefix:@"    Self: "];
                                   printf("\n");

                                   // The self peer is technically a peer, so, shove it on in there
                                   peers[egoPeerID] = egoInformation;

                                   NSArray<NSString *>* includedPeers = egoDynamicInfo[@"included"];
                                   printf("Trusted peers (by me):\n");
                                   if(includedPeers && includedPeers.count > 0) {
                                       [self printPeers:includedPeers egoPeerID:egoPeerID informationOnPeers:peers informationOnCRKs:crks];
                                       [otherPeers removeObjectsInArray:includedPeers];
                                   } else {
                                       printf("    No trusted peers.\n");
                                   }
                                   printf("\n");

                                   NSArray<NSString *>* excludedPeers = egoDynamicInfo[@"excluded"];
                                   printf("Excluded peers (by me):\n");
                                   if(excludedPeers && excludedPeers.count > 0) {
                                       [self printPeers:excludedPeers egoPeerID:egoPeerID informationOnPeers:peers informationOnCRKs:crks];
                                       [otherPeers removeObjectsInArray:excludedPeers];
                                   } else {
                                       printf("    No excluded peers.\n");
                                   }
                                   printf("\n");

                                   printf("Other peers (included/excluded by others):\n");
                                   if(otherPeers.count > 0) {
                                       [self printPeers:otherPeers egoPeerID:egoPeerID informationOnPeers:peers informationOnCRKs:crks];
                                   } else {
                                       printf("    No other peers.\n");
                                   }
                                   printf("\n");

                               } else {
                                   printf("No current identity for this device.\n\n");

                                   if(allPeerIDs.count > 0) {
                                       printf("All peers currently in this account:\n");
                                       [self printPeers:allPeerIDs egoPeerID:nil informationOnPeers:peers informationOnCRKs:crks];
                                   } else {
                                       printf("No peers currently exist for this account.\n");
                                   }
                               }

                               printf("\n");
                           }
                       }
                   }];

    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)recoverUsingBottleID:(NSString *)bottleID
                    entropy:(NSData*)entropy
                  arguments:(OTControlArguments*)arguments
                    control:(OTControl*)control
{
    __block int ret = 1;

#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [control restoreFromBottle:arguments
                       entropy:entropy
                      bottleID:bottleID
                         reply:^(NSError* _Nullable error) {
        if(error) {
            ret = 1;
            fprintf(stderr, "Error recovering: %s\n", [[error description] UTF8String]);
        } else {
            printf("Succeeded recovering bottled peer %s\n", [[bottleID description] UTF8String]);
            ret = 0;
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        fprintf(stderr, "timed out waiting for restore/recover\n");
    }

    return ret;
#else
    ret = 1;
    return ret;
#endif
}

- (int)fetchAllBottles:(OTControlArguments*)arguments
               control:(OTControl*)control {
    __block int ret = 1;

#if OCTAGON
    __block NSError* localError = nil;

    __block NSArray<NSString *>* localViableBottleIDs = nil;
    __block NSArray<NSString *>* localPartiallyViableBottleIDs = nil;

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [control fetchAllViableBottles:arguments
                             reply:^(NSArray<NSString *>* _Nullable sortedBottleIDs,
                                     NSArray<NSString *>* _Nullable sortedPartialBottleIDs,
                                     NSError* _Nullable controlError) {
        if(controlError) {
            secnotice("clique", "findOptimalBottleIDsWithContextData errored: %@\n", controlError);
        } else {
            secnotice("clique", "findOptimalBottleIDsWithContextData succeeded: %@, %@\n", sortedBottleIDs, sortedPartialBottleIDs);
            ret = 0;
        }
        localError = controlError;
        localViableBottleIDs = sortedBottleIDs;
        localPartiallyViableBottleIDs = sortedPartialBottleIDs;
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        secnotice("clique", "findOptimalBottleIDsWithContextData failed to fetch bottles\n");
        return 1;
    }

    [localViableBottleIDs enumerateObjectsUsingBlock:^(NSString * obj, NSUInteger idx, BOOL* stop) {
        printf("preferred bottleID: %s\n", [obj UTF8String]);
    }];

    [localPartiallyViableBottleIDs enumerateObjectsUsingBlock:^(NSString * obj, NSUInteger idx, BOOL* stop) {
        printf("partial recovery bottleID: %s\n", [obj UTF8String]);
    }];

    return ret;
#else
    ret = 1;
    return ret;
#endif
}

- (int)healthCheck:(OTControlArguments*)arguments
skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
{
#if OCTAGON
    __block int ret = 1;

    [self.control healthCheck:arguments
        skipRateLimitingCheck:skipRateLimitingCheck
                        reply:^(NSError* _Nullable error) {
        if(error) {
            fprintf(stderr, "Error checking health: %s\n", [[error description] UTF8String]);
        } else {
            printf("Checking Octagon Health completed.\n");
            ret = 0;
        }
    }];
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)refetchCKKSPolicy:(OTControlArguments*)arguments
{
#if OCTAGON
    __block int ret = 1;

    [self.control refetchCKKSPolicy:arguments
                              reply:^(NSError * _Nullable error) {
        if(error) {
            fprintf(stderr, "Error refetching CKKS policy: %s\n", [[error description] UTF8String]);
        } else {
            printf("CKKS refetch completed.\n");
            ret = 0;
        }
    }];
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)tapToRadar:(NSString *)action description:(NSString *)description radar:(NSString *)radar
{
#if OCTAGON
    __block int ret = 1;

    [self.control tapToRadar:action
                 description:description
                       radar:radar
                       reply:^(NSError* _Nullable error) {
        if(error) {
            fprintf(stderr, "Error trigger TTR: %s\n", [[error description] UTF8String]);
        } else {
            printf("Trigger TTR completed.\n");
            ret = 0;
        }
    }];
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)setUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                  enabled:(BOOL)enabled
{
#if OCTAGON
    __block int ret = 1;

    [self.control setUserControllableViewsSyncStatus:arguments
                                             enabled:enabled
                                               reply:^(BOOL nowSyncing, NSError * _Nullable error) {
        if(error) {
            fprintf(stderr, "Error setting user controllable views: %s\n", [[error description] UTF8String]);
        } else {
            printf("User controllable views are now %s.\n", [(nowSyncing ? @"enabled" : @"paused") UTF8String]);
            ret = 0;
        }
    }];
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)fetchUserControllableViewsSyncStatus:(OTControlArguments*)arguments
{
#if OCTAGON
    __block int ret = 1;

    [self.control fetchUserControllableViewsSyncStatus:arguments
                                                 reply:^(BOOL nowSyncing, NSError * _Nullable error) {
        if(error) {
            fprintf(stderr, "Error setting user controllable views: %s\n", [[error description] UTF8String]);
        } else {
            printf("User controllable views are currently %s.\n", [(nowSyncing ? @"enabled" : @"paused") UTF8String]);
            ret = 0;
        }
    }];
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)resetAccountCDPContentsWithArguments:(OTControlArguments*)arguments
{
    __block int ret = 1;

#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control resetAccountCDPContents:arguments reply:^(NSError * _Nullable error) {
        if(error) {
            fprintf(stderr, "Error resetting account cdp content: %s\n", [[error description] UTF8String]);
        } else {
            printf("Succeeded resetting account cdp content\n");
            ret = 0;
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        fprintf(stderr, "timed out waiting for restore/recover\n");
        ret = 1;
    }

    return ret;
#else
    ret = 1;
    return ret;
#endif
}

- (int)createCustodianRecoveryKeyWithArguments:(OTControlArguments*)arguments
                                          json:(bool)json
                                       timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;
    do {
        retry = false;
        [self.control createCustodianRecoveryKey:arguments
                                            uuid:nil
                                           reply:^(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error) {
            if (error) {
                fprintf(stderr, "createCustodianRecoveryKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                ret = 0;
                if (json) {
                    NSDictionary *d = @{
                        @"uuid": [crk.uuid description],
                        @"recoveryString": crk.recoveryString,
                        @"wrappingKey": [crk.wrappingKey base64EncodedStringWithOptions:0],
                        @"wrappedKey": [crk.wrappedKey base64EncodedStringWithOptions:0],
                    };
                    print_json(d);
                } else {
                    printf("Created custodian key %s, string: %s, wrapping key: %s, wrapped key: %s\n",
                           [[crk.uuid description] UTF8String],
                           [crk.recoveryString UTF8String],
                           [[crk.wrappingKey base64EncodedStringWithOptions:0] UTF8String],
                           [[crk.wrappedKey base64EncodedStringWithOptions:0] UTF8String]);
                }
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)joinWithCustodianRecoveryKeyWithArguments:(OTControlArguments*)arguments
                                     wrappingKey:(NSString*)wrappingKey
                                      wrappedKey:(NSString*)wrappedKey
                                      uuidString:(NSString*)uuidString
                                         timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    NSData *wrappingKeyData = [[NSData alloc] initWithBase64EncodedString:wrappingKey options:0];
    if (wrappingKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappingKey\n");
        return 1;
    }
    NSData *wrappedKeyData = [[NSData alloc] initWithBase64EncodedString:wrappedKey options:0];
    if (wrappedKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappedKey\n");
        return 1;
    }
    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    if (uuid == nil) {
        fprintf(stderr, "bad format for custodianUUID\n");
        return 1;
    }

    NSError *initError = nil;
    OTCustodianRecoveryKey* crk = [[OTCustodianRecoveryKey alloc] initWithWrappedKey:wrappedKeyData
                                                                         wrappingKey:wrappingKeyData
                                                                                uuid:uuid
                                                                               error:&initError];
    if (crk == nil) {
        fprintf(stderr, "failed to create OTCustodianRecoveryKey: %s\n", [[initError description] UTF8String]);
        return 1;
    }

    do {
        retry = false;
        [self.control joinWithCustodianRecoveryKey:arguments
                              custodianRecoveryKey:crk
                                             reply:^(NSError* _Nullable error) {
            if (error) {
                fprintf(stderr, "joinWithCustodianRecoveryKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("successful join from custodian recovery key\n");
                ret = 0;
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)preflightJoinWithCustodianRecoveryKeyWithArguments:(OTControlArguments*)arguments
                                              wrappingKey:(NSString*)wrappingKey
                                               wrappedKey:(NSString*)wrappedKey
                                               uuidString:(NSString*)uuidString
                                                  timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    NSData *wrappingKeyData = [[NSData alloc] initWithBase64EncodedString:wrappingKey options:0];
    if (wrappingKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappingKey\n");
        return 1;
    }
    NSData *wrappedKeyData = [[NSData alloc] initWithBase64EncodedString:wrappedKey options:0];
    if (wrappedKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappedKey\n");
        return 1;
    }
    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    if (uuid == nil) {
        fprintf(stderr, "bad format for custodianUUID\n");
        return 1;
    }

    NSError *initError = nil;
    OTCustodianRecoveryKey* crk = [[OTCustodianRecoveryKey alloc] initWithWrappedKey:wrappedKeyData
                                                                         wrappingKey:wrappingKeyData
                                                                                uuid:uuid
                                                                               error:&initError];
    if (crk == nil) {
        fprintf(stderr, "failed to create OTCustodianRecoveryKey: %s\n", [[initError description] UTF8String]);
        return 1;
    }

    do {
        retry = false;
        [self.control preflightJoinWithCustodianRecoveryKey:arguments
                                       custodianRecoveryKey:crk
                                                      reply:^(NSError* _Nullable error) {
            if (error) {
                fprintf(stderr, "preflightJoinWithCustodianRecoveryKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("successful preflight join from custodian recovery key\n");
                ret = 0;
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)removeCustodianRecoveryKeyWithArguments:(OTControlArguments*)arguments
                                    uuidString:(NSString*)uuidString
                                       timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    if (uuid == nil) {
        fprintf(stderr, "bad format for custodianUUID\n");
        return 1;
    }
    do {
        retry = false;
        [self.control removeCustodianRecoveryKey:arguments
                                            uuid:uuid
                                           reply:^(NSError* _Nullable error) {
            if (error) {
                fprintf(stderr, "remove custodian recovery key failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("successful removal of custodian recovery key\n");
                ret = 0;
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)createInheritanceKeyWithArguments:(OTControlArguments*)arguments
                                    json:(bool)json
                                 timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;
    do {
        retry = false;
        [self.control createInheritanceKey:arguments
                                      uuid:nil
                                     reply:^(OTInheritanceKey *_Nullable ik, NSError *_Nullable error) {
            if (error) {
                fprintf(stderr, "createInheritanceKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                ret = 0;
                if (json) {
                    NSDictionary *d = @{
                        @"uuid": [ik.uuid description],
                        @"wrappingKeyData": [ik.wrappingKeyData base64EncodedStringWithOptions:0],
                        @"wrappingKeyString": ik.wrappingKeyString,
                        @"wrappedKeyData": [ik.wrappedKeyData base64EncodedStringWithOptions:0],
                        @"wrappedKeyString": ik.wrappedKeyString,
                        @"claimTokenData": [ik.claimTokenData base64EncodedStringWithOptions:0],
                        @"claimTokenString": ik.claimTokenString,
                        @"recoveryKeyData": [ik.recoveryKeyData base64EncodedStringWithOptions:0],
                    };
                    print_json(d);
                } else {
                    printf("Created inheritance key %s\n"
                           "\twrappingKeyData: %s\n"
                           "\twrappingKeyString: %s\n"
                           "\twrappedKeyData: %s\n"
                           "\twrappedKeyString: %s\n"
                           "\tclaimTokenData: %s\n"
                           "\tclaimTokenString: %s\n"
                           "\trecoveryKeyData: %s\n",
                           [[ik.uuid description] UTF8String],
                           [[ik.wrappingKeyData base64EncodedStringWithOptions:0] UTF8String],
                           [ik.wrappingKeyString UTF8String],
                           [[ik.wrappedKeyData base64EncodedStringWithOptions:0] UTF8String],
                           [ik.wrappedKeyString UTF8String],
                           [[ik.claimTokenData base64EncodedStringWithOptions:0] UTF8String],
                           [ik.claimTokenString UTF8String],
                           [[ik.recoveryKeyData base64EncodedStringWithOptions:0] UTF8String]
                           );
                }
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)generateInheritanceKeyWithArguments:(OTControlArguments*)arguments
                                      json:(bool)json
                                   timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;
    do {
        retry = false;
        [self.control generateInheritanceKey:arguments
                                        uuid:nil
                                       reply:^(OTInheritanceKey *_Nullable ik, NSError *_Nullable error) {
            if (error) {
                printf("generateInheritanceKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                ret = 0;
                if (json) {
                    NSDictionary *d = @{
                        @"uuid": [ik.uuid description],
                        @"wrappingKeyData": [ik.wrappingKeyData base64EncodedStringWithOptions:0],
                        @"wrappingKeyString": ik.wrappingKeyString,
                        @"wrappedKeyData": [ik.wrappedKeyData base64EncodedStringWithOptions:0],
                        @"wrappedKeyString": ik.wrappedKeyString,
                        @"claimTokenData": [ik.claimTokenData base64EncodedStringWithOptions:0],
                        @"claimTokenString": ik.claimTokenString,
                        @"recoveryKeyData": [ik.recoveryKeyData base64EncodedStringWithOptions:0],
                    };
                    print_json(d);
                } else {
                    printf("Generated inheritance key %s\n"
                           "\twrappingKeyData: %s\n"
                           "\twrappingKeyString: %s\n"
                           "\twrappedKeyData: %s\n"
                           "\twrappedKeyString: %s\n"
                           "\tclaimTokenData: %s\n"
                           "\tclaimTokenString: %s\n"
                           "\trecoveryKeyData: %s\n",
                           [[ik.uuid description] UTF8String],
                           [[ik.wrappingKeyData base64EncodedStringWithOptions:0] UTF8String],
                           [ik.wrappingKeyString UTF8String],
                           [[ik.wrappedKeyData base64EncodedStringWithOptions:0] UTF8String],
                           [ik.wrappedKeyString UTF8String],
                           [[ik.claimTokenData base64EncodedStringWithOptions:0] UTF8String],
                           [ik.claimTokenString UTF8String],
                           [[ik.recoveryKeyData base64EncodedStringWithOptions:0] UTF8String]
                           );
                }
            }
        }];
    } while (retry);
    return ret;
#else
    printf("Unimplemented.\n");
    return 1;
#endif
}

- (int)storeInheritanceKeyWithArguments:(OTControlArguments*)arguments
                            wrappingKey:(NSString*)wrappingKey
                             wrappedKey:(NSString*)wrappedKey
                             uuidString:(NSString*)uuidString
                                timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    NSData *wrappingKeyData = [[NSData alloc] initWithBase64EncodedString:wrappingKey options:0];
    if (wrappingKeyData == nil) {
        printf("bad base64 data for wrappingKey\n");
        return 1;
    }
    NSData *wrappedKeyData = [[NSData alloc] initWithBase64EncodedString:wrappedKey options:0];
    if (wrappedKeyData == nil) {
        printf("bad base64 data for wrappedKey\n");
        return 1;
    }
    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    if (uuid == nil) {
        printf("bad format for inheritanceUUID\n");
        return 1;
    }

    NSError *initError = nil;
    OTInheritanceKey *ik = [[OTInheritanceKey alloc] initWithWrappedKeyData:wrappedKeyData
                                                            wrappingKeyData:wrappingKeyData
                                                                       uuid:uuid
                                                                      error:&initError];
    if (ik == nil) {
        printf("failed to create OTInheritanceKey: %s\n", [[initError description] UTF8String]);
        return 1;
    }

    do {
        retry = false;
        [self.control storeInheritanceKey:arguments
                                       ik:ik
                                    reply:^(NSError* _Nullable error) {
            if (error) {
                printf("storeInheritanceKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("successful store of inheritance key\n");
                ret = 0;
            }
        }];
    } while (retry);
    return ret;
#else
    printf("Unimplemented.\n");
    return 1;
#endif
}

- (int)joinWithInheritanceKeyWithArguments:(OTControlArguments*)arguments
                               wrappingKey:(NSString*)wrappingKey
                                wrappedKey:(NSString*)wrappedKey
                                uuidString:(NSString*)uuidString
                                   timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    NSData *wrappingKeyData = [[NSData alloc] initWithBase64EncodedString:wrappingKey options:0];
    if (wrappingKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappingKey\n");
        return 1;
    }
    NSData *wrappedKeyData = [[NSData alloc] initWithBase64EncodedString:wrappedKey options:0];
    if (wrappedKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappedKey\n");
        return 1;
    }
    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    if (uuid == nil) {
        fprintf(stderr, "bad format for inheritanceUUID\n");
        return 1;
    }

    NSError *initError = nil;
    OTInheritanceKey *ik = [[OTInheritanceKey alloc] initWithWrappedKeyData:wrappedKeyData
                                                            wrappingKeyData:wrappingKeyData
                                                                       uuid:uuid
                                                                      error:&initError];
    if (ik == nil) {
        fprintf(stderr, "failed to create OTInheritanceKey: %s\n", [[initError description] UTF8String]);
        return 1;
    }

    do {
        retry = false;
        [self.control joinWithInheritanceKey:arguments
                              inheritanceKey:ik
                                       reply:^(NSError* _Nullable error) {
            if (error) {
                fprintf(stderr, "joinWithInheritanceKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("successful join from inheritance key\n");
                ret = 0;
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)preflightJoinWithInheritanceKeyWithArguments:(OTControlArguments*)arguments
                                        wrappingKey:(NSString*)wrappingKey
                                         wrappedKey:(NSString*)wrappedKey
                                         uuidString:(NSString*)uuidString
                                            timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    NSData *wrappingKeyData = [[NSData alloc] initWithBase64EncodedString:wrappingKey options:0];
    if (wrappingKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappingKey\n");
        return 1;
    }
    NSData *wrappedKeyData = [[NSData alloc] initWithBase64EncodedString:wrappedKey options:0];
    if (wrappedKeyData == nil) {
        fprintf(stderr, "bad base64 data for wrappedKey\n");
        return 1;
    }
    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    if (uuid == nil) {
        fprintf(stderr, "bad format for inheritanceUUID\n");
        return 1;
    }

    NSError *initError = nil;
    OTInheritanceKey *ik = [[OTInheritanceKey alloc] initWithWrappedKeyData:wrappedKeyData
                                                            wrappingKeyData:wrappingKeyData
                                                                       uuid:uuid
                                                                      error:&initError];
    if (ik == nil) {
        fprintf(stderr, "failed to create OTInheritanceKey: %s\n", [[initError description] UTF8String]);
        return 1;
    }

    do {
        retry = false;
        [self.control preflightJoinWithInheritanceKey:arguments
                                       inheritanceKey:ik
                                                reply:^(NSError* _Nullable error) {
            if (error) {
                fprintf(stderr, "preflight joinWithInheritanceKey failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("successful preflight join from inheritance key\n");
                ret = 0;
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

- (int)removeInheritanceKeyWithArguments:(OTControlArguments*)arguments
                              uuidString:(NSString*)uuidString
                                 timeout:(NSTimeInterval)timeout
{
#if OCTAGON
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
    __block int ret = 1;
    __block bool retry;

    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
    if (uuid == nil) {
        fprintf(stderr, "bad format for inheritanceUUID\n");
        return 1;
    }
    do {
        retry = false;
        [self.control removeInheritanceKey:arguments
                                      uuid:uuid
                                     reply:^(NSError* _Nullable error) {
            if (error) {
                fprintf(stderr, "remove inheritance key failed: %s\n", [[error description] UTF8String]);
                if ([deadline timeIntervalSinceNow] > 0 && [error isRetryable]) {
                    retry = true;
                    sleep([error retryInterval]);
                }
            } else {
                printf("successful removal of inheritance key\n");
                ret = 0;
            }
        }];
    } while (retry);
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}


- (int)setMachineIDOverride:(OTControlArguments*)arguments
                  machineID:(NSString*)machineID
                       json:(bool)json
{
#if OCTAGON
    __block int ret = 1;
    
    [self.control setMachineIDOverride:arguments machineID:machineID reply:^(NSError * _Nullable replyError) {
        if (replyError) {
            if(json) {
                print_json(@{@"error" : [replyError description]});
            } else {
                fprintf(stderr, "Failed to set machineID override: %s\n", [[replyError description] UTF8String]);
            }
        } else {
            printf("successfully set machineID override!\n");
            ret = 0;
        }
    }];
    
    return ret;
#else
    fprintf(stderr, "Unimplemented.\n");
    return 1;
#endif
}

@end
