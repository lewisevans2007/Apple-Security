//
//  SOSTransportCircleCK.m
//  Security
//
//
//

#import <Foundation/Foundation.h>
#import "keychain/SecureObjectSync/SOSTransport.h"
#import "keychain/SecureObjectSync/SOSAccountPriv.h"
#import "SOSTransportCircleCK.h"

@implementation SOSCKCircleStorage

-(id) init
{
    if ((self = [super init])) {
        SOSRegisterTransportCircle(self);
    }
    return self;
}

-(id) initWithAccount:(SOSAccount*)acct
{
    if ((self = [super init])) {
        self.account = acct;
    }
    return self;
}

-(CFIndex) getTransportType
{
    return kCK;
}
-(SOSAccount*) getAccount
{
    return self.account;
}

-(bool) expireRetirementRecords:(CFDictionaryRef) retirements err:(CFErrorRef *)error
{
    return true;
}

-(bool) flushChanges:(CFErrorRef *)error
{
    return true;
}
-(bool) postCircle:(CFStringRef)circleName circleData:(CFDataRef)circle_data err:(CFErrorRef *)error
{
    return true;
}
-(bool) postRetirement:(CFStringRef) circleName peer:(SOSPeerInfoRef) peer err:(CFErrorRef *)error
{
    return true;
}

-(CFDictionaryRef)handleRetirementMessages:(CFMutableDictionaryRef) circle_retirement_messages_table err:(CFErrorRef *)error
{
    return NULL;
}
-(CFArrayRef)CF_RETURNS_RETAINED handleCircleMessagesAndReturnHandledCopy:(CFMutableDictionaryRef) circle_circle_messages_table err:(CFErrorRef *)error
{
    return NULL;
}

@end
