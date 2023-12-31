#if OCTAGON

import Security_Private.OTClique_Private
import XCTest

@objcMembers
class OctagonEscrowRecordTests: OctagonTestsBase {

    func testFetchEscrowRecord() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        OctagonSetPlatformSupportsSOS(true)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords: [OTEscrowRecord] = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow record")
            let reduced = escrowRecords.compactMap { $0.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
    }

    func testViableBottleCachingAfterJoin() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        OctagonSetPlatformSupportsSOS(true)

        // now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        // now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow record")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 1)
    }

    func testCachedEscrowRecordFetch() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        OctagonSetPlatformSupportsSOS(true)

        // now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        // now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 1)

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(bottlerContext.activeAccount))
        container.escrowCacheTimeout = 1

        // sleep to invalidate the cache
        sleep(1)

        // now call fetchviablebottles, we should get the uncached version
        let uncachedViableBottlesFetchExpectation = self.expectation(description: "fetch Uncached ViableBottles")
        let fetchBottlesFromCuttlefishFetchExpectation = self.expectation(description: "fetch bottles from cuttlefish expectation")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchBottlesFromCuttlefishFetchExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
            uncachedViableBottlesFetchExpectation.fulfill()
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchBottlesFromCuttlefishFetchExpectation], timeout: 10)
        self.wait(for: [uncachedViableBottlesFetchExpectation], timeout: 10)
    }

    func testForcedEscrowRecordFetch() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        bottlerotcliqueContext.overrideEscrowCache = false

        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        OctagonSetPlatformSupportsSOS(true)

        // now call fetchviablebottles, we should get records from cuttlefish
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        // set the override to force an escrow record fetch
        bottlerotcliqueContext.overrideEscrowCache = true

        // now call fetchviablebottles, we should get records from cuttlefish
        let fetchViableBottlesExpectation = self.expectation(description: "fetch forced ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 10)
    }

    func testSignInWithEscrowPrecachingEnabled() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName
        OctagonSetPlatformSupportsSOS(true)

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        do {
            // expect fetch escrow record fetch
            let fetchViableBottlesExpectation = self.expectation(description: "fetchViableBottles occurs")

#if os(tvOS)
            // aTV/HomePods should _not_ perform this fetch: they can't use escrow records anyway
            fetchViableBottlesExpectation.isInverted = true
#endif

            self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
                self.fakeCuttlefishServer.fetchViableBottlesListener = nil
                XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
                fetchViableBottlesExpectation.fulfill()
                return nil
            }

            let signInExpectation = self.expectation(description: "signing in expectation")

            self.manager.appleAccountSigned(in: OTControlArguments(containerName: containerName, contextID: contextName, altDSID: newAltDSID)) { error in
                XCTAssertNil(error, "error should not be nil")
                signInExpectation.fulfill()
            }

            self.wait(for: [signInExpectation], timeout: 10)
            self.wait(for: [fetchViableBottlesExpectation], timeout: 2)
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        // And restarting don't cause a refetch
        self.fakeCuttlefishServer.fetchViableBottlesListener = { _ in
            XCTFail("Do not expect a fetchViableBottles")
            return nil
        }

        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        do {
            let signOutExpectation = self.expectation(description: "signed out expectation")

            self.manager.appleAccountSignedOut(OTControlArguments(containerName: containerName, contextID: contextName, altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))) { error in
                XCTAssertNil(error, "error should not be nil")
                signOutExpectation.fulfill()
            }

            self.wait(for: [signOutExpectation], timeout: 10)

            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        }

        // And signin in again will preload the cache
        do {
            let fetchViableBottlesExpectation = self.expectation(description: "fetchViableBottles occurs")

#if os(tvOS)
            // aTV/HomePods should _not_ perform this fetch: they can't use escrow records anyway
            fetchViableBottlesExpectation.isInverted = true
#endif

            self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
                self.fakeCuttlefishServer.fetchViableBottlesListener = nil
                XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
                fetchViableBottlesExpectation.fulfill()
                return nil
            }

            let signInExpectation = self.expectation(description: "signing in expectation")

            self.manager.appleAccountSigned(in: OTControlArguments(containerName: containerName, contextID: contextName, altDSID: newAltDSID)) { error in
                XCTAssertNil(error, "error should not be nil")
                signInExpectation.fulfill()
            }

            self.wait(for: [signInExpectation], timeout: 10)
            self.wait(for: [fetchViableBottlesExpectation], timeout: 2)
        }
    }

    func testLegacyEscrowRecordFetch() throws {
        OctagonSetPlatformSupportsSOS(true)

        self.startCKAccountStatusMock()

        let initiatorContextID = "joiner"
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = OTDefaultContext
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        // SOS TLK shares will be uploaded after the establish
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()

        let joinerContext = self.makeInitiatorContext(contextID: initiatorContextID)
        self.assertJoinViaEscrowRecovery(joiningContext: joinerContext, sponsor: self.cuttlefishContext)

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))

        // now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.injectLegacyEscrowRecords = true
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")

            XCTAssertEqual(escrowRecords.count, 3, "should be 3 escrow records")
            let recordsWithBottles = escrowRecords.filter { !$0!.escrowInformationMetadata.bottleId.isEmpty }
            XCTAssertEqual(recordsWithBottles.count, 2, "should be 2 escrow records with a bottleID")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        // now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")

            container.moc.performAndWait {
                let legacy = container.containerMO.legacyEscrowRecords as! Set<EscrowRecordMO>
                let partial = container.containerMO.partiallyViableEscrowRecords as! Set<EscrowRecordMO>
                let full = container.containerMO.fullyViableEscrowRecords as! Set<EscrowRecordMO>

                XCTAssertEqual(legacy.count, 1, "legacy escrowRecords should contain 1 record")
                XCTAssertEqual(partial.count, 1, "partially viable escrowRecords should contain 1 record")
                XCTAssertEqual(full.count, 1, "fully viable escrowRecords should contain 1 record")
            }

            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")

            XCTAssertEqual(escrowRecords.count, 3, "should be 3 escrow record")
            let recordsWithBottles = escrowRecords.filter { !$0!.escrowInformationMetadata.bottleId.isEmpty }
            XCTAssertEqual(recordsWithBottles.count, 2, "should be 2 escrow records with a bottleID")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 1)

        // check cache is empty after escrow fetch timeout expires
        container.escrowCacheTimeout = 1
        sleep(1)

        // now call fetchviablebottles, we should get the uncached version, check there's 0 cached records
        let fetchViableBottlesAfterExpiredTimeoutExpectation = self.expectation(description: "fetch Cached ViableBottles expectaiton after timeout")
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")

            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")
            }

            fetchViableBottlesAfterExpiredTimeoutExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 3, "should be 3 escrow record")
            let recordsWithBottles = escrowRecords.filter { !$0!.escrowInformationMetadata.bottleId.isEmpty }
            XCTAssertEqual(recordsWithBottles.count, 2, "should be 2 escrow records with a bottleID")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesAfterExpiredTimeoutExpectation], timeout: 10)
    }

    func testEmptyEscrowRecords() throws {
        self.fakeCuttlefishServer.includeEscrowRecords = false
        self.fakeCuttlefishServer.injectLegacyEscrowRecords = false

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(bottlerContext.activeAccount))

        OctagonSetPlatformSupportsSOS(true)

        let fetchViableBottlesAfterExpiredTimeoutExpectation = self.expectation(description: "fetch Cached ViableBottles expectaiton after timeout")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")

            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")
            }
            fetchViableBottlesAfterExpiredTimeoutExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            XCTAssertEqual(escrowRecordDatas.count, 0, "should be 0 escrow records")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesAfterExpiredTimeoutExpectation], timeout: 10)
        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")
        }
    }

    func testRemoveEscrowCache() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        OctagonSetPlatformSupportsSOS(true)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords: [OTEscrowRecord] = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow record")
            let reduced = escrowRecords.compactMap { $0.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        let removeExpectation = self.expectation(description: "remove expectation")
        self.manager.invalidateEscrowCache(OTControlArguments(configuration: bottlerotcliqueContext)) { error in
            XCTAssertNil(error, "error should not be nil")
            removeExpectation.fulfill()
        }
        self.wait(for: [removeExpectation], timeout: 10)

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(bottlerContext.activeAccount))

        let fetchViableBottlesAfterCacheRemovalExpectation = self.expectation(description: "fetchViableBottles expectation after cache removal")
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")

            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")
            }

            fetchViableBottlesAfterCacheRemovalExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesAfterCacheRemovalExpectation], timeout: 10)
    }

    func testFetchViableBottlesFilteringOctagonOnly() throws {
        OctagonSetPlatformSupportsSOS(false)

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords: [OTEscrowRecord] = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow record")
            let reduced = escrowRecords.compactMap { $0.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        let removeExpectation = self.expectation(description: "remove expectation")
        self.manager.invalidateEscrowCache(OTControlArguments(configuration: bottlerotcliqueContext)) { error in
            XCTAssertNil(error, "error should not be nil")
            removeExpectation.fulfill()
        }
        self.wait(for: [removeExpectation], timeout: 10)

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(bottlerContext.activeAccount))

        let fetchViableBottlesAfterCacheRemovalExpectation = self.expectation(description: "fetchViableBottles expectation after cache removal")
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .byOctagonOnly, "request filtering should be unknown")
            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")
            }
            fetchViableBottlesAfterCacheRemovalExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesAfterCacheRemovalExpectation], timeout: 10)
    }

    func setupTLKRecoverability(contextID: String) throws {
        let bottlerContext = self.makeInitiatorContext(contextID: contextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = contextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        OctagonSetPlatformSupportsSOS(true)
    }

    func testTLKRecoverabilityAllRecordsValid() throws {
        let initiatorContextID = "initiator-context-id"
        try self.setupTLKRecoverability(contextID: initiatorContextID)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl

        // now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)

            var tlkRecoverabilityExpectation = self.expectation(description: "recoverability expectation")
            self.manager.tlkRecoverability(forEscrowRecordData: OTControlArguments(configuration: self.otcliqueContext), record: escrowRecordDatas[0]) {retViews, error in
                XCTAssertNotNil(retViews, "retViews should not be nil")
#if !os(watchOS) && !os(tvOS)
                XCTAssertTrue(retViews!.contains("Manatee"), "should contain Manatee view")
#endif
                XCTAssertTrue(retViews!.contains("LimitedPeersAllowed"), "should contain LimitedPeersAllowed view")
                XCTAssertNil(error, "error should be nil")
                tlkRecoverabilityExpectation.fulfill()
            }
            self.wait(for: [tlkRecoverabilityExpectation], timeout: 10)

            tlkRecoverabilityExpectation = self.expectation(description: "recoverability expectation")
            self.manager.tlkRecoverability(forEscrowRecordData: OTControlArguments(configuration: self.otcliqueContext), record: escrowRecordDatas[1]) {retViews, error in
                XCTAssertNotNil(retViews, "retViews should not be nil")
#if !os(watchOS) && !os(tvOS)
                XCTAssertTrue(retViews!.contains("Manatee"), "should contain Manatee view")
#endif
                XCTAssertTrue(retViews!.contains("LimitedPeersAllowed"), "should contain LimitedPeersAllowed view")
                XCTAssertNil(error, "error should be nil")
                tlkRecoverabilityExpectation.fulfill()
            }
            self.wait(for: [tlkRecoverabilityExpectation], timeout: 10)
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
    }

    func testTLKRecoverabilityNoneValid() throws {
        let initiatorContextID = "initiator-context-id"
        try self.setupTLKRecoverability(contextID: initiatorContextID)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl

        // now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .unknown, "request filtering should be unknown")
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)

            let resetExpectation = self.expectation(description: "resetExpectation")
            self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
                XCTAssertNil(resetError, "should NOT error resetting and establishing")
                resetExpectation.fulfill()
            }
            self.wait(for: [resetExpectation], timeout: 10)

            var tlkRecoverabilityExpectation = self.expectation(description: "recoverability expectation")
            self.manager.tlkRecoverability(forEscrowRecordData: OTControlArguments(configuration: self.otcliqueContext), record: escrowRecordDatas[0]) {retViews, error in
                XCTAssertNil(retViews, "retViews should be nil")
                XCTAssertNotNil(error, "error should not be nil")
                XCTAssertEqual((error! as NSError).code, 58, "error code should be 58")
                XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
                tlkRecoverabilityExpectation.fulfill()
            }
            self.wait(for: [tlkRecoverabilityExpectation], timeout: 10)

            tlkRecoverabilityExpectation = self.expectation(description: "recoverability expectation")
            self.manager.tlkRecoverability(forEscrowRecordData: OTControlArguments(configuration: self.otcliqueContext), record: escrowRecordDatas[1]) {retViews, error in
                XCTAssertNil(retViews, "retViews should be nil")
                XCTAssertNotNil(error, "error should not be nil")
                XCTAssertEqual((error! as NSError).code, 58, "error code should be 58")
                XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
                tlkRecoverabilityExpectation.fulfill()
            }
            self.wait(for: [tlkRecoverabilityExpectation], timeout: 10)
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
    }
    func testTLKRecoverabilityOneViableOneNotViable() throws {
        OctagonSetPlatformSupportsSOS(false)
        let initiatorContextID = "initiator-context-id"
        try self.setupTLKRecoverability(contextID: initiatorContextID)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = OTDefaultContext
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl

        let clique = OTClique(contextData: bottlerotcliqueContext)
        OctagonSetPlatformSupportsSOS(false)
        XCTAssertNoThrow(try clique.leave(), "Should be NO error departing clique")

        // now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(request.filterRequest, .byOctagonOnly, "request filtering should be byOctagonOnly")
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)

            var tlkRecoverabilityExpectation = self.expectation(description: "recoverability expectation")
            self.manager.tlkRecoverability(forEscrowRecordData: OTControlArguments(configuration: self.otcliqueContext), record: escrowRecordDatas[0]) {retViews, error in
                if retViews == nil {
                    XCTAssertNil(retViews, "retViews should be nil")
                    XCTAssertNotNil(error, "error should not be nil")
                    XCTAssertEqual((error! as NSError).code, 58, "error code should be 58")
                    XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
                } else {
                    XCTAssertNotNil(retViews, "retViews should not be nil")
#if !os(watchOS) && !os(tvOS)
                XCTAssertTrue(retViews!.contains("Manatee"), "should contain Manatee view")
#endif
                XCTAssertTrue(retViews!.contains("LimitedPeersAllowed"), "should contain LimitedPeersAllowed view")
                    XCTAssertNil(error, "error should be nil")
                }
                tlkRecoverabilityExpectation.fulfill()
            }
            self.wait(for: [tlkRecoverabilityExpectation], timeout: 10)

            tlkRecoverabilityExpectation = self.expectation(description: "recoverability expectation")
            self.manager.tlkRecoverability(forEscrowRecordData: OTControlArguments(configuration: self.otcliqueContext), record: escrowRecordDatas[1]) {retViews, error in
                if retViews == nil {
                    XCTAssertNil(retViews, "retViews should be nil")
                    XCTAssertNotNil(error, "error should not be nil")
                    XCTAssertEqual((error! as NSError).code, 58, "error code should be 58")
                    XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
                } else {
                    XCTAssertNotNil(retViews, "retViews should not be nil")
#if !os(watchOS) && !os(tvOS)
                XCTAssertTrue(retViews!.contains("Manatee"), "should contain Manatee view")
#endif
                XCTAssertTrue(retViews!.contains("LimitedPeersAllowed"), "should contain LimitedPeersAllowed view")
                    XCTAssertNil(error, "error should be nil")
                }
                tlkRecoverabilityExpectation.fulfill()
            }
            self.wait(for: [tlkRecoverabilityExpectation], timeout: 10)
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
    }
}

#endif
