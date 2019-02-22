//-----------------------------------------------------------------------------
// Copyright (C) 2019 Merlok
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// MIFARE Application Directory (MAD) functions
//-----------------------------------------------------------------------------

#include "mad.h"
#include "ui.h"
#include "crc.h"
#include "util.h"

// https://www.nxp.com/docs/en/application-note/AN10787.pdf
static madAIDDescr madKnownAIDs[] = {
	{0x0000, "free"}, 
	{0x0001, "defect, e.g. access keys are destroyed or unknown"},
	{0x0002, "reserved"},
	{0x0003, "contains additional directory info"},
	{0x0004, "contains card holder information in ASCII format."},
	{0x0005, "not applicable (above memory size)"},

	{0x03e1, "NDEF"},
};

static madAIDDescr madKnownClusterCodes[] = {
	{0x00, "cluster: card administration"},
	{0x01, "cluster: miscellaneous applications"},
	{0x02, "cluster: miscellaneous applications"},
	{0x03, "cluster: miscellaneous applications"},
	{0x04, "cluster: miscellaneous applications"},
	{0x05, "cluster: miscellaneous applications"},
	{0x06, "cluster: miscellaneous applications"},
	{0x07, "cluster: miscellaneous applications"},
	{0x08, "cluster: airlines"},
	{0x09, "cluster: ferry traffic"},
	{0x10, "cluster: railway services"},
	{0x11, "cluster: miscellaneous applications"},
	{0x12, "cluster: transport"},
	{0x14, "cluster: security solutions"},
	{0x18, "cluster: city traffic"},
	{0x19, "cluster: Czech Railways"},
	{0x20, "cluster: bus services"},
	{0x21, "cluster: multi modal transit"},
	{0x28, "cluster: taxi"},
	{0x30, "cluster: road toll"},
	{0x31, "cluster: generic transport"},
	{0x38, "cluster: company services"},
	{0x40, "cluster: city card services"},
	{0x47, "cluster: access control & security"},
	{0x48, "cluster: access control & security"},
	{0x49, "cluster: VIGIK"},
	{0x4A, "cluster: Ministry of Defence, Netherlands"},
	{0x4B, "cluster: Bosch Telecom, Germany"},
	{0x4C, "cluster: European Union Institutions"},
	{0x50, "cluster: ski ticketing"},
	{0x51, "cluster: access control & security"},
	{0x52, "cluster: access control & security"},
	{0x53, "cluster: access control & security"},
	{0x54, "cluster: access control & security"},
	{0x55, "cluster: SOAA standard for offline access standard"},
	{0x56, "cluster: access control & security"},
	{0x58, "cluster: academic services"},
	{0x60, "cluster: food"},
	{0x68, "cluster: non-food trade"},
	{0x70, "cluster: hotel"},
	{0x71, "cluster: loyalty"},
	{0x75, "cluster: airport services"},
	{0x78, "cluster: car rental"},
	{0x79, "cluster: Dutch government"},
	{0x80, "cluster: administration services"},
	{0x88, "cluster: electronic purse"},
	{0x90, "cluster: television"},
	{0x91, "cluster: cruise ship"},
	{0x95, "cluster: IOPTA"},
	{0x97, "cluster: metering"},
	{0x98, "cluster: telephone"},
	{0xA0, "cluster: health services"},
	{0xA8, "cluster: warehouse"},
	{0xB0, "cluster: electronic trade"},
	{0xB8, "cluster: banking"},
	{0xC0, "cluster: entertainment & sports"},
	{0xC8, "cluster: car parking"},
	{0xC9, "cluster: fleet management"},
	{0xD0, "cluster: fuel, gasoline"},
	{0xD8, "cluster: info services"},
	{0xE0, "cluster: press"},
	{0xE1, "cluster: NFC Forum"},
	{0xE8, "cluster: computer"},
	{0xF0, "cluster: mail"},
	{0xF8, "cluster: miscellaneous applications"},
};

static const char unknownAID[] = "";

static const char *GetAIDDescription(uint16_t AID) {
	for(int i = 0; i < ARRAYLEN(madKnownAIDs); i++)
		if (madKnownAIDs[i].AID == AID)
			return madKnownAIDs[i].Description;

	for(int i = 0; i < ARRAYLEN(madKnownClusterCodes); i++)
		if (madKnownClusterCodes[i].AID == AID)
			return madKnownClusterCodes[i].Description;
		
	return unknownAID;
}

int madCRCCheck(uint8_t *sector, bool verbose, int MADver) {
	if (MADver == 1) {
		uint8_t crc = CRC8Mad(&sector[16 + 1], 31);
		if (crc != sector[16]) {
			if (verbose)
				PrintAndLogEx(ERR, "Wrong MAD%d CRC. Calculated: 0x%02x, from card: 0x%02x", MADver, crc, sector[16]);
			return 3;
		};
	} else {
	}
	
	return 0;
}

uint16_t madGetAID(uint8_t *sector, int MADver, int sectorNo) {
	return (sector[16 + 2 + (sectorNo - 1) * 2] << 8) + (sector[16 + 2 + (sectorNo - 1) * 2 + 1]);	
}

int MAD1DecodeAndPrint(uint8_t *sector, bool verbose, bool *haveMAD2) {
	
	uint8_t GPB = sector[3 * 16 + 9];
	PrintAndLogEx(NORMAL, "GPB: 0x%02x", GPB);
	
	// DA (MAD available)
	if (!(GPB & 0x80)) {
		PrintAndLogEx(ERR, "DA=0! MAD not available.");
		return 1;
	}
	
	// MA (multi-application card)
	if (GPB & 0x40)
		PrintAndLogEx(NORMAL, "Multi application card.");
	else
		PrintAndLogEx(NORMAL, "Single application card.");
	
	uint8_t MADVer = GPB & 0x03;
	PrintAndLogEx(NORMAL, "MAD version: %d", MADVer);
	
	//  MAD version
	if ((MADVer != 0x01) && (MADVer != 0x02)) {
		PrintAndLogEx(ERR, "Wrong MAD version: 0x%02x", MADVer);
		return 2;
	};
	
	if (haveMAD2)
		*haveMAD2 = (MADVer == 2);

	int res = madCRCCheck(sector, true, 1);
	if (res)
		return res;	
	
	if (verbose)
		PrintAndLogEx(NORMAL, "CRC8-MAD OK.");
	
	// info byte
	uint8_t InfoByte = sector[16 + 1] & 0x3f;
	if (InfoByte) {
		PrintAndLogEx(NORMAL, "Card publisher sector: 0x%02x", InfoByte);
	} else {
		if (verbose)
			PrintAndLogEx(NORMAL, "Card publisher sector not present.");
	}
	if (InfoByte == 0x10 || InfoByte >= 0x28)
		PrintAndLogEx(WARNING, "Info byte error");
	
	for(int i = 1; i < 16; i++) {
		uint16_t AID = madGetAID(sector, 1, i);
		PrintAndLogEx(NORMAL, "%02d [%04X] %s", i, AID, GetAIDDescription(AID));
	};
		
	return 0;
};

int MAD2DecodeAndPrint(uint8_t *sector, bool verbose) {
	int res = madCRCCheck(sector, true, 2);
	if (res)
		return res;	

	
	return 0;
};
