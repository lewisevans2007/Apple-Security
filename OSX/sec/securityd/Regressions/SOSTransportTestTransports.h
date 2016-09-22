#ifndef SEC_SOSTransportTestTransports_h
#define SEC_SOSTransportTestTransports_h

typedef struct SOSTransportKeyParameterTest *SOSTransportKeyParameterTestRef;
typedef struct SOSTransportCircleTest *SOSTransportCircleTestRef;
typedef struct SOSTransportMessageTest *SOSTransportMessageTestRef;
typedef struct SOSTransportMessageIDSTest *SOSTransportMessageIDSTestRef;

CF_RETURNS_RETAINED
CFDictionaryRef SOSTransportMessageTestHandleMessages(SOSTransportMessageTestRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);

void SOSAccountUpdateTestTransports(SOSAccountRef account, CFDictionaryRef gestalt);
    
SOSTransportKeyParameterTestRef SOSTransportTestCreateKeyParameter(SOSAccountRef account, CFStringRef name, CFStringRef circleName);
SOSTransportCircleTestRef SOSTransportTestCreateCircle(SOSAccountRef account, CFStringRef name, CFStringRef circleName);
SOSTransportMessageTestRef SOSTransportTestCreateMessage(SOSAccountRef account, CFStringRef name, CFStringRef circleName);
bool SOSTransportCircleTestRemovePendingChange(SOSTransportCircleRef transport,  CFStringRef circleName, CFErrorRef *error);

extern CFMutableArrayRef key_transports;
extern CFMutableArrayRef circle_transports;
extern CFMutableArrayRef message_transports;

CFStringRef SOSTransportMessageTestGetName(SOSTransportMessageTestRef transport);
CFStringRef SOSTransportCircleTestGetName(SOSTransportCircleTestRef transport);
CFStringRef SOSTransportKeyParameterTestGetName(SOSTransportKeyParameterTestRef transport);

void SOSTransportKeyParameterTestSetName(SOSTransportKeyParameterTestRef transport, CFStringRef accountName);
void SOSTransportCircleTestSetName(SOSTransportCircleTestRef transport, CFStringRef accountName);
void SOSTransportMessageTestSetName(SOSTransportMessageTestRef transport, CFStringRef accountName);


CFMutableDictionaryRef SOSTransportMessageTestGetChanges(SOSTransportMessageTestRef transport);
CFMutableDictionaryRef SOSTransportCircleTestGetChanges(SOSTransportCircleTestRef transport);
CFMutableDictionaryRef SOSTransportKeyParameterTestGetChanges(SOSTransportKeyParameterTestRef transport);

SOSAccountRef SOSTransportMessageTestGetAccount(SOSTransportMessageRef transport);
SOSAccountRef SOSTransportCircleTestGetAccount(SOSTransportCircleTestRef transport);
SOSAccountRef SOSTransportKeyParameterTestGetAccount(SOSTransportKeyParameterTestRef transport);

bool SOSAccountInflateTestTransportsForCircle(SOSAccountRef account, CFStringRef circleName, CFStringRef accountName, CFErrorRef *error);
bool SOSAccountEnsureFactoryCirclesTest(SOSAccountRef a, CFStringRef accountName);
void SOSTransportKeyParameterTestClearChanges(SOSTransportKeyParameterTestRef transport);
void SOSTransportCircleTestClearChanges(SOSTransportCircleTestRef transport);
void SOSTransportMessageTestClearChanges(SOSTransportMessageTestRef transport);




//Test IDS transport
SOSTransportMessageIDSTestRef SOSTransportMessageIDSTestCreate(SOSAccountRef account, CFStringRef accountName, CFStringRef circleName, CFErrorRef *error);
CFMutableDictionaryRef SOSTransportMessageIDSTestGetChanges(SOSTransportMessageRef transport);
void SOSTransportMessageIDSTestSetName(SOSTransportMessageRef transport, CFStringRef accountName);
CFStringRef SOSTransportMessageIDSTestGetName(SOSTransportMessageRef transport);
void SOSTransportMessageIDSTestClearChanges(SOSTransportMessageRef transport);


#endif