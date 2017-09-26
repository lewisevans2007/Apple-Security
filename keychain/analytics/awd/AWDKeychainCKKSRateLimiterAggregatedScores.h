// This file was automatically generated by protocompiler
// DO NOT EDIT!
// Compiled from stdin

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBCodable.h>

#ifdef __cplusplus
#define AWDKEYCHAINCKKSRATELIMITERAGGREGATEDSCORES_FUNCTION extern "C"
#else
#define AWDKEYCHAINCKKSRATELIMITERAGGREGATEDSCORES_FUNCTION extern
#endif

@interface AWDKeychainCKKSRateLimiterAggregatedScores : PBCodable <NSCopying>
{
    PBRepeatedUInt32 _datas;
    uint64_t _timestamp;
    NSString *_ratelimitertype;
    struct {
        int timestamp:1;
    } _has;
}


@property (nonatomic) BOOL hasTimestamp;
@property (nonatomic) uint64_t timestamp;

@property (nonatomic, readonly) NSUInteger datasCount;
@property (nonatomic, readonly) uint32_t *datas;
- (void)clearDatas;
- (void)addData:(uint32_t)i;
- (uint32_t)dataAtIndex:(NSUInteger)idx;
- (void)setDatas:(uint32_t *)list count:(NSUInteger)count;

@property (nonatomic, readonly) BOOL hasRatelimitertype;
@property (nonatomic, retain) NSString *ratelimitertype;

// Performs a shallow copy into other
- (void)copyTo:(AWDKeychainCKKSRateLimiterAggregatedScores *)other;

// Performs a deep merge from other into self
// If set in other, singular values in self are replaced in self
// Singular composite values are recursively merged
// Repeated values from other are appended to repeated values in self
- (void)mergeFrom:(AWDKeychainCKKSRateLimiterAggregatedScores *)other;

AWDKEYCHAINCKKSRATELIMITERAGGREGATEDSCORES_FUNCTION BOOL AWDKeychainCKKSRateLimiterAggregatedScoresReadFrom(__unsafe_unretained AWDKeychainCKKSRateLimiterAggregatedScores *self, __unsafe_unretained PBDataReader *reader);

@end
