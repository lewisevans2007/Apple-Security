// This file was automatically generated by protocompiler
// DO NOT EDIT!
// Compiled from SecDbBackupRecoverySet.proto

#import "SecDbBackupBagIdentity.h"
#import <ProtocolBuffer/PBConstants.h>
#import <ProtocolBuffer/PBHashUtil.h>
#import <ProtocolBuffer/PBDataReader.h>

#if !__has_feature(objc_arc)
# error This generated file depends on ARC but it is not enabled; turn on ARC, or use 'objc_use_arc' option to generate non-ARC code.
#endif

@implementation SecDbBackupBagIdentity

- (BOOL)hasBaguuid
{
    return _baguuid != nil;
}
@synthesize baguuid = _baguuid;
- (BOOL)hasBaghash
{
    return _baghash != nil;
}
@synthesize baghash = _baghash;

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ %@", [super description], [self dictionaryRepresentation]];
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (self->_baguuid)
    {
        [dict setObject:self->_baguuid forKey:@"baguuid"];
    }
    if (self->_baghash)
    {
        [dict setObject:self->_baghash forKey:@"baghash"];
    }
    return dict;
}

BOOL SecDbBackupBagIdentityReadFrom(__unsafe_unretained SecDbBackupBagIdentity *self, __unsafe_unretained PBDataReader *reader) {
    while (PBReaderHasMoreData(reader)) {
        uint32_t tag = 0;
        uint8_t aType = 0;

        PBReaderReadTag32AndType(reader, &tag, &aType);

        if (PBReaderHasError(reader))
            break;

        if (aType == TYPE_END_GROUP) {
            break;
        }

        switch (tag) {

            case 1 /* baguuid */:
            {
                NSData *new_baguuid = PBReaderReadData(reader);
                self->_baguuid = new_baguuid;
            }
            break;
            case 2 /* baghash */:
            {
                NSData *new_baghash = PBReaderReadData(reader);
                self->_baghash = new_baghash;
            }
            break;
            default:
                if (!PBReaderSkipValueWithTag(reader, tag, aType))
                    return NO;
                break;
        }
    }
    return !PBReaderHasError(reader);
}

- (BOOL)readFrom:(PBDataReader *)reader
{
    return SecDbBackupBagIdentityReadFrom(self, reader);
}
- (void)writeTo:(PBDataWriter *)writer
{
    /* baguuid */
    {
        if (self->_baguuid)
        {
            PBDataWriterWriteDataField(writer, self->_baguuid, 1);
        }
    }
    /* baghash */
    {
        if (self->_baghash)
        {
            PBDataWriterWriteDataField(writer, self->_baghash, 2);
        }
    }
}

- (void)copyTo:(SecDbBackupBagIdentity *)other
{
    if (_baguuid)
    {
        other.baguuid = _baguuid;
    }
    if (_baghash)
    {
        other.baghash = _baghash;
    }
}

- (id)copyWithZone:(NSZone *)zone
{
    SecDbBackupBagIdentity *copy = [[[self class] allocWithZone:zone] init];
    copy->_baguuid = [_baguuid copyWithZone:zone];
    copy->_baghash = [_baghash copyWithZone:zone];
    return copy;
}

- (BOOL)isEqual:(id)object
{
    SecDbBackupBagIdentity *other = (SecDbBackupBagIdentity *)object;
    return [other isMemberOfClass:[self class]]
    &&
    ((!self->_baguuid && !other->_baguuid) || [self->_baguuid isEqual:other->_baguuid])
    &&
    ((!self->_baghash && !other->_baghash) || [self->_baghash isEqual:other->_baghash])
    ;
}

- (NSUInteger)hash
{
    return 0
    ^
    [self->_baguuid hash]
    ^
    [self->_baghash hash]
    ;
}

- (void)mergeFrom:(SecDbBackupBagIdentity *)other
{
    if (other->_baguuid)
    {
        [self setBaguuid:other->_baguuid];
    }
    if (other->_baghash)
    {
        [self setBaghash:other->_baghash];
    }
}

@end
