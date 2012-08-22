//TODO: UPDATE TO ACTUAL STATE

#if !defined(struc_h)
#define struc_h

#include "defines.h"

#include <time.h>
#include <memory.h>
#include <vector>

using namespace std;

class CCanDevice;
struct _IP_DEV_DATA;

/* ==============  COMMAND =============== */
#define	HEADER_LEN 	14

#pragma pack(1)
typedef struct _COMMAND
{
  UCHAR CommandID;		// 1
  UCHAR bAnswer;		// 1
  DWORD iMasterID;		// 4
  DWORD iAddr;			// 4
  DWORD DataLen;		// 4
  UCHAR Data[BUF_LEN];		// 512
  _COMMAND()
  {
    CommandID	=
    bAnswer	= 0;
    iMasterID	=
    iAddr	=
    DataLen	= 0;
    memset(Data, 0, sizeof(Data));
  }
}  __attribute__ ((packed)) COMMAND;


//#pragma pack(1)
typedef struct _DEVLIST
{
  UCHAR sn[8];
  USHORT Addr;
  _DEVLIST()
  {
    memset(sn, 0, sizeof(sn));
    Addr = 0;
  }
} __attribute__ ((packed))  DEVLIST;


//#pragma pack(1)
typedef struct _LISTDATA
{
  DWORD Version;
  DWORD CRC;
  int CardCountAll;
  int CardCountLock;
  int CardCountUnlock;
  struct _cd // carddata structure has 2 first bytes reserved , last 8 are used
  {
    UCHAR sn[10];
    UCHAR status;
  } __attribute__ ((packed)) carddata[11000];
} __attribute__ ((packed)) LISTDATA;


/* ============= FLASHEVENT ================ */
//#pragma pack(1)
typedef struct _FLASHEVENT	// 66 bytes
{
  ULONG EventNumber; 		// 4
  ULONG Time;			// 4
  USHORT HallID; 		// 2
  UCHAR HallDeviceID;   	// 1
  UCHAR HallDeviceType; 	// 1
  UCHAR EventCode;		// 1
  UCHAR ErrorCode;		// 1
  UCHAR DataLen;		// 1
  UCHAR EventData[51];		// 51
  /*_FLASHEVENT()
  {
    EventNumber 	=
    Time 		=
    HallID 		= 0;
    HallDeviceID 	=
    HallDeviceType 	=
    EventCode 		=
    ErrorCode 		=
    DataLen 		= 0;
    memset(EventData, 0, sizeof(EventData));
  }*/
} __attribute__ ((packed)) FLASHEVENT;


/* ============= EVENT ================ */
//#pragma pack(1)
typedef struct _EVENT // 84
{
  ULONG  main_id;	// 4
  USHORT addr;		// 2
  UCHAR  sn[8];		// 8
  ULONG  regTime; 	// 4
  FLASHEVENT event;	// 66
  _EVENT()
  {
    main_id 	=
    addr 	=
    regTime 	= 0;
    memset(sn, 0, sizeof(sn));
  }
  friend bool operator< (const _EVENT &left, const _EVENT &right)
  	{return (left.main_id<right.main_id);}
} __attribute__ ((packed)) EVENT;

/* ============= EVENT_SINGLE_BYTE ============== 
 * (0x10 | 16) 
 * (0x20 | 32)  
 * */
typedef struct _EVENT_SINGLE_BYTE
{
	UCHAR Counter;
} __attribute__ ((packed)) EVENT_SINGLE_BYTE;

/* ============= EVENT_PASS_PERMIT ============== 0x11 0x12 | 17 18 */
typedef struct _EVENT_PASS_PERMIT
{
	UCHAR Counter;
	ULONG UserCardSN;
} __attribute__ ((packed)) EVENT_PASS_PERMIT;

/* ============= EVENT_SAVE_COUNTERS_VALUE ============== 0x50 | 80 */
typedef struct _EVENT_SAVE_COUNTERS_VALUE
{
	UCHAR EventVer;
	UCHAR CountCode;
	UCHAR CountData;
} __attribute__ ((packed)) EVENT_SAVE_COUNTERS_VALUE;

/* ============= EVENT_ARM_INIT_OPEN_SESSION ==== 150 151 */
typedef struct _EVENT_ARM_INIT_OPEN_SESSION
{
	ULONG CashCardSN;
	UCHAR CashASPPSN[10];
	ULONG SessionID;
	UCHAR SessionType;
} __attribute__ ((packed)) EVENT_ARM_INIT_OPEN_SESSION;

/* ============= EVENT_CARD_PROCESSING ==== 153 */
typedef struct _EVENT_CARD_PROCESSING
{
	ULONG SessionID;
	UCHAR SessionType;
	ULONG UserCardSN;
	UCHAR UserASPPSN[10];
	UCHAR EventID;
	UCHAR IsCardInit;
	ULONG ContractValue;
	ULONG ContractCopyID;
} __attribute__ ((packed)) EVENT_CARD_PROCESSING;

/* ============= EVENT_CARD_PROCESSING2 ==== 154 */
typedef struct _EVENT_CARD_PROCESSING2
{
	UCHAR EventVer;
	ULONG SessionID;
	UCHAR SessionType;
	USHORT UserCardType;
	UCHAR BitMapVer;
	UINT64 UserCardSN;
	UCHAR UserASPPSN[10];
	UCHAR EventID;
	UCHAR IsCardInit;
	UCHAR AID;
	UCHAR PIX;
	ULONG ContractValue;
	ULONG ContractCopyID;
} __attribute__ ((packed)) EVENT_CARD_PROCESSING2;

/* ============= EVENT_PASS_PERMIT2 ( 0x13 ) ================ */
//#pragma pack(1)
typedef struct _EVENT_PASS_PERMIT2
{
  UCHAR EventVer;                  // 1
  UCHAR Counter;                   // 1
  USHORT UserCardType;             // 2
  UCHAR BitMapVer;                 // 1
  UINT64 UserCardSN;               // 8
	
#ifndef PURE_STRUCTURES

  _EVENT_PASS_PERMIT2()
  {
    EventVer =
    Counter =
    UserCardType =
    BitMapVer =
    UserCardSN = 0;
  }
  char* ToString()
  {
        return "дозвіл проходу по MifareUltraLight";
  }
	
#endif

} __attribute__ ((packed)) EVENT_PASS_PERMIT2;


/* ============= EVENT_STOP_LIST (100/101) ================ */
//#pragma pack(1)
typedef struct _EVENT_STOP_LIST	// 14 bytes
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];		// 10
	
#ifndef PURE_STRUCTURES	
	
  _EVENT_STOP_LIST()
  {
    UserCardSN = 0;
    memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
	
#endif	
	
} __attribute__ ((packed)) EVENT_STOP_LIST;

/* ============= EVENT_STOP_LIST2 (102) ================ */
//#pragma pack(1)
typedef struct _EVENT_STOP_LIST2 // 24 bytes
{
  UCHAR EventVer;		// 1
  USHORT UserCardType;		// 2
  UINT64 UserCardSN;            // 8
  UCHAR UserASPPSN[10];		// 10
  UCHAR CodeEvent;		// 1
  USHORT TCTransactionNumber;   // 2
	
#ifndef PURE_STRUCTURES
	
  _EVENT_STOP_LIST2()
  {
    UserCardSN = 0;
    memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
	
#endif

} __attribute__ ((packed)) EVENT_STOP_LIST2;

/* ============= EVENT_CONTRACT_ADD (206) ================ */
//#pragma pack(1)
typedef struct _EVENT_CONTRACT_ADD		// 40 bytes
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];		// 10
  ULONG CashCardSN;		// 4
  UCHAR CashASPPSN[10];		// 10
  USHORT ContractID;		// 2
  ULONG ContractValue;		// 4
  USHORT TransactionValue;	// 2
  ULONG Amount;			// 4
	
#ifndef PURE_STRUCTURES

  _EVENT_CONTRACT_ADD()
  {
	UserCardSN=
	CashCardSN=
	ContractID=
	ContractValue=
	TransactionValue=
	Amount=0;
       memset(CashASPPSN, 0, sizeof(CashASPPSN));
       memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
  char* ToString()
  {
  	return "поповнення контракту";
  }
	
#endif

} __attribute__ ((packed)) EVENT_CONTRACT_ADD;

/* ============= EVENT_CONTRACT_ADD2 (226) ================ */
//#pragma pack(1)
typedef struct _EVENT_CONTRACT_ADD2  // 40
{
  UCHAR EventVer;                  // 1
  ULONG CashCardSN;                // 4
  USHORT UserCardType;             // 2
  UCHAR BitMapVer;                 // 1
  UINT64 UserCardSN;               // 8
  UCHAR UserASPPSN[10];            // 10
  USHORT AID;                      // 2
  USHORT PIX;                      // 2
  ULONG ContractValue;		   // 4
  USHORT TransactionValue;		   // 2 // was ValData
  UCHAR CodeEvent;		// 1
  ULONG Amount;                    // 4
  USHORT ContractTransactionNumber_Metro;  // 2
  USHORT TCTransactionNumber;		// 2

#ifndef PURE_STRUCTURES
	
  _EVENT_CONTRACT_ADD2()
  {
    EventVer=0;
    CashCardSN=0;
    UserCardType=0;
    BitMapVer=0;
    UserCardSN=0;
    memset(UserASPPSN, 0, sizeof(UserASPPSN));
    AID=0;
    PIX=0;
    ContractValue=0;
    TransactionValue=0;
    CodeEvent=0;
    Amount=0;
    ContractTransactionNumber_Metro=0;
    TCTransactionNumber=0;
  }
  char* ToString()
  {
  	return "поповнення контракту (2)";
  }
	
#endif

} __attribute__ ((packed)) EVENT_CONTRACT_ADD2;

/* ============= EVENT_CONTRACT_USE (207) ================ */
//#pragma pack(1)
typedef struct _EVENT_CONTRACT_USE		// 22
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];		// 10
  USHORT ContractID;		// 2
  ULONG ContractValue;		// 4
  USHORT TransactionValue;	// 2
	
#ifndef PURE_STRUCTURES	

  _EVENT_CONTRACT_USE()
  {
	UserCardSN=
	ContractID=
	ContractValue=
	TransactionValue=0;
       memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
  char* ToString()
  {
  	return "використання контракту";
  }
	
#endif
} __attribute__ ((packed)) EVENT_CONTRACT_USE;


/* ============= EVENT_CONTRACT_USE (227) ================ */
//#pragma pack(1)
typedef struct _EVENT_CONTRACT_USE2       // 36
{
  UCHAR EventVer;                  // 1
  USHORT UserCardType;             // 2
  UCHAR BitMapVer;                 // 1
  UINT64 UserCardSN;               // 8
  UCHAR UserASPPSN[10];            // 10
  USHORT AID;                      // 2
  USHORT PIX;                      // 2
  ULONG ContractValue;		   // 4
  USHORT ValidityData;             // 2
  USHORT ContractTransactionNumber_Metro; // 2
  USHORT TCTransactionNumber; // 2
	
#ifndef PURE_STRUCTURES
	
  _EVENT_CONTRACT_USE2()
  {
    EventVer=0;
    UserCardType=0;
    BitMapVer=0;
    UserCardSN=0;
    memset(UserASPPSN, 0, sizeof(UserASPPSN));
    AID=0;
    PIX=0;
    ValidityData=0;
    ContractTransactionNumber_Metro=0;
  }
  char* ToString()
  {
  	return "використання контракту UltraLight";
  }

#endif

} __attribute__ ((packed)) EVENT_CONTRACT_USE2;


// ============= EVENT_CONTRACT_ZALOG_ADD (208) ================
typedef struct _EVENT_CONTRACT_ZALOG_ADD		// 32 bytes
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];		// 10
  ULONG CashCardSN;		// 4
  UCHAR CashASPPSN[10];		// 10
  ULONG MortValue;		// 4
	
#ifndef PURE_STRUCTURES
	
  _EVENT_CONTRACT_ZALOG_ADD()
  {
		UserCardSN=
		CashCardSN=
		MortValue=0;
		memset(CashASPPSN, 0, sizeof(CashASPPSN));
		memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
  char* ToString()
  {
  	return "отримання завдатку БК";
  }
	
#endif
	
} __attribute__ ((packed)) EVENT_CONTRACT_ZALOG_ADD;

// ============= EVENT_CONTRACT_ZALOG_REMOVE (209) ================
typedef struct _EVENT_CONTRACT_ZALOG_RM: EVENT_CONTRACT_ZALOG_ADD		// 32 bytes
{
  char* ToString()
  {
  	return "повернення завдатку БК";
  }
} __attribute__ ((packed)) EVENT_CONTRACT_ZALOG_RM;

// ============= EVENT_CONTRACT_ZALOG (215) ================
typedef struct _EVENT_CONTRACT_ZALOG //40 bytes
{
	UCHAR EventVer;                  // 1
	ULONG CashCardSN;		// 4
	UINT64 UserCardSN;               // 8
	UCHAR UserASPPSN[10];		// 10
	UCHAR CodeEvent;		// 1
	ULONG MortValue;		// 4
	USHORT TCTransactionNumber;		// 2

#ifndef PURE_STRUCTURES

	_EVENT_CONTRACT_ZALOG()
	{
		memset(this,0,sizeof(this));
	}

	char* ToString()
  	{
  		return "операція завдатку БК";
	}
	
#endif

} __attribute__ ((packed)) EVENT_CONTRACT_ZALOG;


/* ============= EVENT_WRITE_CONTRACT (210) ================ */
//#pragma pack(1)
typedef struct _EVENT_WRITE_CONTRACT		// 30 bytes
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];	// 10
  ULONG CashCardSN;		// 4
  UCHAR CashASPPSN[10];	// 10
  USHORT ContractID;		// 2
  _EVENT_WRITE_CONTRACT()
  {
	UserCardSN=
	CashCardSN=
	ContractID=0;
       memset(CashASPPSN, 0, sizeof(CashASPPSN));
       memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
  char* ToString()
  {
  	return "ініціювання (запис) контракту";
  }
} __attribute__ ((packed)) EVENT_WRITE_CONTRACT;

// ============= EVENT_CONTRACT (230) ================
typedef struct _EVENT_CONTRACT // 40
{
  UCHAR EventVer;                  // 1
  UINT64 UserCardSN;               // 8
  UCHAR UserASPPSN[10];		// 10
  ULONG CashCardSN;                // 4
  UCHAR CodeEvent;		// 1
  USHORT AID;                      // 2
  USHORT PIX;                      // 2
  ULONG ContractValue;		   // 4
  ULONG ContractCopyID;		   // 4
  USHORT ContractTransactionNumber_Metro; // 2
  USHORT TCTransactionNumber; // 2

#ifndef PURE_STRUCTURES

  _EVENT_CONTRACT() {
	EventVer=0;
	UserCardSN=0;
	memset(UserASPPSN,0,sizeof(UserASPPSN));
	CashCardSN=0;
	CodeEvent=0;
	AID=0;
	PIX=0;
	ContractValue=0;
	ContractCopyID=0;
	ContractTransactionNumber_Metro=0;
	TCTransactionNumber=0;
}

  char* ToString()
  {
  	return "операція з контрактом";
  }

#endif

} __attribute__ ((packed)) EVENT_CONTRACT;

// ============= EVENT_DELETE_CONTRACT (211) ================
typedef struct _EVENT_DELETE_CONTRACT		// 30 bytes
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];	// 10
  ULONG CashCardSN;		// 4
  UCHAR CashASPPSN[10];	// 10
  USHORT ContractID;		// 2
  ULONG ContractValue;		// 4
  ULONG ContractCopyID;		// 4
	
#ifndef PURE_STRUCTURES
	
  _EVENT_DELETE_CONTRACT()
  {
		UserCardSN=
		CashCardSN=
		ContractID=
		ContractValue=
		ContractCopyID=0;
		memset(CashASPPSN, 0, sizeof(CashASPPSN));
		memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
  char* ToString()
  {
  	return "видалення контракту";
  }
	
#endif
	
} __attribute__ ((packed)) EVENT_DELETE_CONTRACT;

// ==============  EVENT_WALLET_OPERATION (212) ================
typedef struct _EVENT_WALLET_OPERATION
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];		// 10
  ULONG CashCardSN;		// 4
  UCHAR CashASPPSN[10];		// 10
  USHORT ContractID;		// 2
  ULONG ContractValue;		// 4
  USHORT TransactionValue;	// 2
  ULONG Amount;			// 4
  ULONG ContractCopyID;		// 4
	
#ifndef PURE_STRUCTURES
	
  _EVENT_WALLET_OPERATION()
  {
		UserCardSN=
		CashCardSN=
		ContractID=
		ContractCopyID=
		ContractValue=
		TransactionValue=
		Amount=0;
		memset(CashASPPSN, 0, sizeof(CashASPPSN));
		memset(UserASPPSN, 0, sizeof(UserASPPSN));
	}
  char* ToString()
  {
  	return "операцiя електронного гаманця";
  }

#endif

} __attribute__ ((packed)) EVENT_WALLET_OPERATION;

// ==============  EVENT_CONTRACT_SUBT (213) ================
typedef struct _EVENT_CONTRACT_SUBT
{
  ULONG UserCardSN;		// 4
  UCHAR UserASPPSN[10];		// 10
  USHORT ContractID;		// 2
  ULONG ContractValue;		// 4
  USHORT TransactionValue;	// 2
  ULONG ContractCopyID;		// 4
	
#ifndef PURE_STRUCTURES
	
  _EVENT_CONTRACT_SUBT()
  {
	UserCardSN=
	ContractID=
	ContractCopyID=
	ContractValue=
	TransactionValue=0;
	memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
  char* ToString()
  {
  	return "повернення контракту";
  }

#endif

} __attribute__ ((packed)) EVENT_CONTRACT_SUBT;

// ==============  EVENT_CONTRACT_SUBT2 (216) ================
typedef struct _EVENT_CONTRACT_SUBT2 // 43
{
  UCHAR EventVer;		// 1
  UINT64 UserCardSN;		// 8
  UCHAR UserASPPSN[10];		// 10
  ULONG CashCardSN;		// 4
  USHORT ContractID;		// 2
  ULONG ContractValue;		// 4
  USHORT TransactionValue;	// 2
  ULONG ContractCopyID;		// 4
  ULONG Amount;			// 4
  USHORT ContractTransactionNumber_Metro; // 2
  USHORT TCTransactionNumber; // 2
	
#ifndef PURE_STRUCTURES
	
  _EVENT_CONTRACT_SUBT2()
  {
	UserCardSN=
	ContractID=
	ContractCopyID=
	ContractValue=
	TransactionValue=0;
	memset(UserASPPSN, 0, sizeof(UserASPPSN));
  }
  char* ToString()
  {
  	return "повернення контракту (2)";
  }
	
#endif
	
} __attribute__ ((packed)) EVENT_CONTRACT_SUBT2;

// ==============  EVENT_WALLET_OPERATION2 (214) ================
typedef struct _EVENT_WALLET_OPERATION2 // 39
{
	UCHAR EventVer;		// 1
	USHORT UserCardType;	// 2
	UCHAR BitMapVer;	// 1
	UINT64 UserCardSN;	// 8
	UCHAR UserASPPSN[10];	// 10
	ULONG CashCardSN;	// 4
	UCHAR TransactionType;	// 1
	ULONG Amount;		// 4
	ULONG PurseValue;	// 4
	USHORT PurseTransactionNumber; // 2
	USHORT TCTransactionNumber; // 2
	
#ifndef PURE_STRUCTURES
	
	_EVENT_WALLET_OPERATION2()
	{
  	    
	}
	char* ToString()
	{
  	    return "операції транспортного гаманця";
	}

#endif

} __attribute__ ((packed)) EVENT_WALLET_OPERATION2;

// ==============  EVENT_ENCASHMENT_ADBK (220) ================
typedef struct _EVENT_ENCASHMENT_ADBK //18
{
	ULONG CashCardSN;
	UCHAR CashASPPSN[10];
	ULONG Amount;
	
#ifndef PURE_STRUCTURES
	
	_EVENT_ENCASHMENT_ADBK()
	{
		CashCardSN=
		Amount=0;
		memset(CashASPPSN, 0, sizeof(CashASPPSN));
	}
  char* ToString()
  {
  	return "Iнкасацiя АДБК";
  }

#endif

} __attribute__ ((packed)) EVENT_ENCASHMENT_ADBK;

// ==============  EVENT_ENCASHMENT_ADBK (231) ================
typedef struct _EVENT_ENCASHMENT_ADBK2 //19
{
	UCHAR EventVer;		// 1
	ULONG CashCardSN;	// 4
	UCHAR CashASPPSN[10];	// 10
	ULONG Amount;		// 4
	
#ifndef PURE_STRUCTURES
	
	_EVENT_ENCASHMENT_ADBK2()
	{
		EventVer=0;
		CashCardSN=Amount=0;
		memset(CashASPPSN, 0, sizeof(CashASPPSN));
	}
  char* ToString()
  {
  	return "Iнкасацiя АДБК";
  }

#endif

} __attribute__ ((packed)) EVENT_ENCASHMENT_ADBK2;

// ============== EVENT_DIAG (221) ==================
typedef struct _EVENT_DIAG // 28
{
	UCHAR EventVer;		// 1
	UINT64 FirmwareVer; // 8
	ULONG CashCardSN; // 4
	UCHAR CashASPPSN[10]; // 10
	UCHAR	StatusCode; // 1
	ULONG Amount; // 4
} __attribute__ ((packed)) EVENT_DIAG;

// ============== EVENT_SET_TIME (222) ==================
typedef struct _EVENT_SET_TIME // 15
{
	UCHAR EventVer;		// 1
	UCHAR EventID;		// 1
	ULONG CurrentTime;	// 4
	ULONG NewTime;		// 4
	UCHAR TimeShift;	// 1
	ULONG GetTime;		// 4
	
#ifndef PURE_STRUCTURES
	
	_EVENT_SET_TIME()
	{
		EventVer=
 		EventID=
		CurrentTime=
		NewTime=
		TimeShift=
		GetTime=0;
	}
	char* ToString()
	{
		return "Встановлення часу на пристрій";
	}

#endif

} __attribute__ ((packed)) EVENT_SET_TIME;

// ==============  EVENT_SALE_CONTRACT (224) ================
typedef struct _EVENT_SALE_CONTRACT		// 5 bytes
{
    ULONG UserCardSN;		// 4
    UCHAR UserASPPSN[10];	// 10
    ULONG CashCardSN;
    UCHAR CashASPPSN[10];
    USHORT ContractID;		// 2
    ULONG ContractValue;	// 4
    ULONG ContractCopyID;	// 4
		
#ifndef PURE_STRUCTURES
		
    _EVENT_SALE_CONTRACT()
    {
	UserCardSN=
	CashCardSN=
	ContractID=
	ContractCopyID=
	ContractValue=0;
	memset(CashASPPSN, 0, sizeof(CashASPPSN));
	memset(UserASPPSN, 0, sizeof(UserASPPSN));
    }
    char* ToString()
    {
	return "продаж контракту";
    }
		
#endif
		
} __attribute__ ((packed)) EVENT_SALE_CONTRACT;

/* ==============  EVENT_ERROR_CONTRACT (255) ================ */
//#pragma pack(1)
typedef struct _EVENT_ERROR_CONTRACT		// 5 bytes
{
  ULONG UserCardSN;		// 4
  UCHAR ErrorEventCode;	// 1
	
#ifndef PURE_STRUCTURES
	
  _EVENT_ERROR_CONTRACT()
  {
	UserCardSN=
	ErrorEventCode=0;
  }

char* ToString()
    {
	return "помилка контракту";
    }
		
#endif

} __attribute__ ((packed)) EVENT_ERROR_CONTRACT;

/* ==============  EVENT_ERROR_CONTRACT2 (254) ================ */
//#pragma pack(1)
typedef struct _EVENT_ERROR_CONTRACT2 	// 22 bytes
{
  UCHAR EventVer;		// 1
  USHORT UserCardType;		// 2
  UINT64 UserCardSN;	// 8
  UCHAR UserASPPSN[10];	// 10
  UCHAR ErrorEventCode;	// 1
	
#ifndef PURE_STRUCTURES
	
  _EVENT_ERROR_CONTRACT2()
  {
	UserCardSN=
	ErrorEventCode=0;
  }

 char* ToString()
    {
	return "помилка контракту (2)";
    }
		
#endif

} __attribute__ ((packed)) EVENT_ERROR_CONTRACT2;

/* ==============  DATE and TIME structures CLOCKDATA =============== */
#pragma pack(1)
typedef struct _DATE
{
  USHORT year :5;
  USHORT month :4;
  USHORT day :5;
  USHORT empty :2;
  _DATE()
  {
    year  =
    month =
    day   =
    empty = 0;
  }
} __attribute__ ((packed)) DATE;

#pragma pack(1)
typedef struct _TIME
{
  USHORT hour :5;
  USHORT minute :6;
  USHORT second :5;
  _TIME()
  {
    hour   =
    minute =
    second = 0;
  }
} __attribute__ ((packed)) TIME;

#pragma pack(1)
typedef struct _CLOCKDATA	// 4 bytes
{
  TIME time;
  DATE date;

  _CLOCKDATA& operator=(const struct tm now)
  {
	this->time.hour 	= now.tm_hour;
	this->time.minute 	= now.tm_min;
	this->time.second 	= now.tm_sec/2;
	this->date.year 	= now.tm_year-100;
	this->date.month 	= now.tm_mon+1;
	this->date.day		= now.tm_mday;
	this->date.empty	= 0;
	return *this;
  }

  _CLOCKDATA()
  {
    TIME();
    DATE();
  }

  _CLOCKDATA(ULONG udt)
  {
    memcpy(this, &udt, sizeof(ULONG));
  }

} __attribute__ ((packed)) CLOCKDATA;


/* ============================================ */
/*                 CAN_DEV_DATA                 */
/* ============================================ */
//#pragma pack(1)
typedef struct _CAN_DEV_DATA	// 53 bytes  //-> 57 with InactiveCount
{
  WORD		addr;		// 2
  UCHAR 	sn[8];		// 8
  WORD		type;		// 2
  UCHAR 	length;		// 1
  UCHAR 	data[8];	// 8
  UCHAR 	ver[10];	// 10
  UCHAR 	bAnswer;	// 1
  UCHAR 	bActive;	// 1
  ULONG 	lastTransNo; 	// 4
// --------- new variables
  CLOCKDATA 	lastTime;			// 4
  WORD		PIX[3];				// 6
  WORD		ContractStopListVersion;	// 2
  ULONG		Flag;				// /4 reserved
  ULONG		InactiveCount;	// /4 // counter for unanswered requests

  void reportActiveState(UCHAR state) {
	#define INACTIVE_LIMIT	8
	//bActive = state;
	if(state) {
		bActive=state;
		InactiveCount = 0;
	} else {
		if(++InactiveCount > INACTIVE_LIMIT) bActive = 0;
	}	
  }

  _CAN_DEV_DATA()
  {
    addr 		=
    type 		=
    length 		=
    bAnswer 		=
    bActive 		=
    PIX[0] 		=
    PIX[1]		=
    PIX[2]		=
    Flag		=
    InactiveCount	=
    ContractStopListVersion=0;

    lastTransNo 	=1;

    memset(sn, 0, sizeof(sn));
    memset(data, 0, sizeof(data));
    memset(ver, 0, sizeof(ver));
  }

  BOOL friend operator==(_CAN_DEV_DATA dev, WORD addrFind)
	{ return (dev.addr == addrFind);}

  friend bool operator< (const _CAN_DEV_DATA &left, const _CAN_DEV_DATA &right)
	{ return (left.addr < right.addr);}

  _CAN_DEV_DATA& operator=(const _IP_DEV_DATA& src)
  {
    memcpy(this, &src, sizeof(_CAN_DEV_DATA));
/*
    if (src.addr>0 && src.type==dtADBK)
    {
      addr = src.addr;
      memcpy(sn, src.sn, sizeof(sn));
      type = src.type;
      length = src.length;
      memcpy(data, src.data, sizeof(data));
      memcpy(ver, src.ver, sizeof(ver));
      bAnswer = src.bAnswer;
      bActive = src.bActive;
      lastTransNo = src.lastTransNo;
    }
*/
    return *this;
  }
} __attribute__ ((packed)) CAN_DEV_DATA;


#pragma pack(1)
typedef struct _CASH_EVENT
{
  UCHAR user_id[10];
  UCHAR card_id[10];
  DWORD amount;
  _CASH_EVENT()
  {
    memset(user_id, 0, sizeof(user_id));
    memset(card_id, 0, sizeof(card_id));
    amount = 0;
  }
} __attribute__ ((packed)) CASH_EVENT;


#pragma pack(1)
typedef struct _CONNECTIONS
{
  int fd;
  int time;
  _CONNECTIONS()
  {
    fd   =
    time = 0;
  }
} __attribute__ ((packed)) CONNECTIONS;


typedef struct _PACK_PARAMETERS
{
  vector<CONNECTIONS>* connections;
  vector<COMMAND>* commands;
  vector<COMMAND>* answers;
  CCanDevice * pDev;
} PACK_PARAMETERS;


//---------------------------------------------------------------------------
//   		***	ADBK structures   ***
//---------------------------------------------------------------------------

//adbk state
const char
  stWaiting   = 1,
  stNeedLogin = 2,
  stWorking   = 3;

//packet commands
const char
  cmdGetLastEventNo	= 1,  //
  cmdGetEvents		= 2,  //
  cmdGetState		= 3,  //
  cmdUpdate		= 4,  //
  cmdRestore		= 5,  //осстановить предыдущую версию программ
  cmdRestart		= 6,  //
  cmdSyncTime		= 7;  //становить время


//Return values
const char
  answOK              = 0,
  answErrTimeOut      = 1,
  answErrUnknownCmd   = 2,
  answErrUpdate       = 3,
  answErrConnect      = 4,
  answBadParam        = 5,
  answBusy            = 6,
  answNumbersOK       = 7,
  answNotRunning      = 8,
  answErrDB           = 9;
  //etc.

//Типы пакетов
const char
  ptCommand      = 0,
  ptAnswer       = 1;

//Типы блоков
const char
  btNone         = 0, // нет блоков
  btEvent        = 1, // блок ответа
  btState        = 2, // блок ответа
  btGetNumbers   = 3, // блок ответа
  btFile         = 4, // блок команды
  btTime         = 5, // блок команды
  btSetNumbers   = 6, // блок команды
  btLimit        = 7, // блок команды/ответа
  btCntrState    = 8; // блок команды

//#pragma pack(1)
typedef struct _SADBKList
{
  USHORT	addr;   // address(YY) - 11, 12, 13 ... : 10.1.XXX.YY
  UCHAR	sn[8];  // serial number
  ULONG	lastID; // number of last transaction
  _SADBKList()
  {
    addr	=
    lastID	= 0;
    memset(sn, 0, sizeof(sn));
  };
} __attribute__ ((packed)) SADBKList;

//#pragma pack(1)
typedef struct _SCommand
{
  char  	cmd;    	//команда
  ulong		event;  	//№ события
  int		count;  	//кол-во (- предыдущие, +последующие)
  char  	block_type;   //тип блока
  int   	block_count;  //кол-во передаваемых блоков
  _SCommand() : cmd(0), event(0), count(0), block_type(btNone),  block_count(0) {};
} __attribute__ ((packed)) SCommand;

//#pragma pack(1)
typedef struct _SAnswer
{
  char		err_code;    	//код ошибки
  char		block_type;  	//тип блока
  int		block_count; 	//кол-во возвращаемых блоков
  _SAnswer() : err_code(answOK), block_type(ptAnswer), block_count(0) {}; // cmd(0),
} __attribute__ ((packed)) SAnswer;


//блок состояния
//---------------------------------------------------------------------------
#pragma pack(1)
typedef struct _SBillAcceptor	// 1 + 1 + 4 = 6
{
  bool  enabled;
  char  err_code;
  ULONG money;
  _SBillAcceptor(): enabled(false), err_code(0), money(0) {};
} __attribute__ ((packed)) SBillAcceptor;

#pragma pack(1)
typedef struct _SCardReader		// 1 + 1 + 8 + 8 = 18
{
  bool  enabled;
  char  err_code;
  unsigned char hw_ver[8];
  unsigned char sn[8];
  _SCardReader()
  {
	enabled=false;
	err_code=0;
	memset(hw_ver, 0, sizeof(hw_ver));
	memset(sn, 0, sizeof(sn));
  }
} __attribute__ ((packed)) SCardReader;

#pragma pack(1)
typedef struct _SVideoSystem	// 1 + 1 = 2
{
  bool  enabled;
  char  err_code;
  _SVideoSystem()
  {
    enabled=false;
    err_code=0;
  }
} __attribute__ ((packed)) SVideoSystem;

#pragma pack(1)
typedef struct _SPrinter		// 1 + 1 + 4 = 6
{
  bool  enabled;
  char  err_code;
  int   check_remain;
  _SPrinter()
  {
    enabled=false;
    err_code=0;
    check_remain=0;
  }
} __attribute__ ((packed)) SPrinter;

#pragma pack(1)
typedef struct _SStateBlock		// 32 + 10 + 1 + 4 + 1
{
  SBillAcceptor bill;		// 6
  SCardReader   card;		// 18
  SVideoSystem  video;		// 2
  SPrinter      printer;	// 6
  unsigned char sw_ver[10];
  char state;	// текущее состояние см. const stXXXXXX
  _SStateBlock()
  {
    memset(sw_ver, 0, sizeof(sw_ver));
    state		= 0;
  }
} __attribute__ ((packed)) SStateBlock;
//---------------------------------------------------------------------------

typedef struct _IP_DEV_DATA: CAN_DEV_DATA
{
  SStateBlock state;
  _IP_DEV_DATA(): CAN_DEV_DATA()
  {
    this->lastTransNo = 0;
  }

  _IP_DEV_DATA& operator = (const CAN_DEV_DATA& src)
  {
    if (src.addr>0 && src.type==dtADBK)
    {
      addr 	= src.addr;
      memcpy(sn, src.sn, sizeof(sn));
      type 	= src.type;
      length 	= src.length;
      memcpy(data, src.data, sizeof(data));
      memcpy(ver, src.ver, sizeof(ver));
      bAnswer = src.bAnswer;
      bActive = src.bActive;
      lastTransNo = src.lastTransNo;
      //InactiveCount = src.InactiveCount; // !Q!
    }
    return *this;
  }
} __attribute__ ((packed)) IP_DEV_DATA;


typedef struct _PACKET_COMM
{
  ULONG len;
  UCHAR type;
  SCommand comm;
  USHORT crc;
} __attribute__ ((packed)) PACKET_COMM;

//---------------------------------------------------------------------------
//Синхронизация даты - времени
typedef struct _STimeBlock    //  ptTime
{
  CLOCKDATA date_time;
} __attribute__ ((packed)) STimeBlock;

//---------------------------------------------------------------------------
//Для встановлення статусу контракт_в
typedef struct _SContractStatusBlock
{
  unsigned short  ContractIDAID; //-- _дентиф_катор контракту AID (поле зарезервовано)
  unsigned short  ContractIDPIX; //-- _дентиф_катор контракту PIX
  unsigned char   ContractStatus;//-- поточний статус контракту
                                 //-- 0x00 контракт активується
                                 //-- 0x01 контракт переходить в стан оч_кування
                                 //-- 0xff контракт  переходить в стан не активного
} __attribute__ ((packed)) SContractStatusBlock;

//---------------------------------------------------------------------------
//Для встановлення д_апазон_встатусу контракт_в
typedef struct _SContractRangeBlock
{
  unsigned short  ContractIDAID; //-- _дентиф_катор контракту AID (поле зарезервовано)
  unsigned short  ContractIDPIX; //-- _дентиф_катор контракту PIX
  unsigned int    RangeBegin;    //-- початок д_апазону
  unsigned short  RangeLength;   //-- довжина д_апазону
} __attribute__ ((packed)) SContractRangeBlock;

//---------------------------------------------------------------------------
typedef struct _SBadContractBlock
{
  unsigned short  ContractIDAID; //-- _дентиф_катор контракту AID (поле зарезервовано)
  unsigned short  ContractIDPIX; //-- _дентиф_катор контракту PIX
                                 //--  дан_, що передаються master-пристроєм,
//     Передаеться _дентиф_катор контракту, у якого вичерпано один (чи обидва) диапазона номер_в
//     Або 0х00000000 - якщо номери не вичерпано у жодного з контракт_в
} __attribute__ ((packed)) SBadContractBlock;

//---------------------------------------------------------------------------
//Для встановлення/отримування л_м_ту
typedef struct _SCashLimitBlock
{
  unsigned int Cash;         //-- поточне значення л_м_ту продажу, у коп_йках
} __attribute__ ((packed)) SCashLimitBlock;


typedef struct _SFileBlock     	//  в SCommand event - update mode
{
  char           file_name[NAME_MAX_LEN];
  char           md5[MD5SUM_LEN]; 	//128-bit hash key
  _SFileBlock()
  {
    memset(file_name, 0, sizeof(file_name));
    memset(md5, 0, sizeof(md5));
  }
} __attribute__ ((packed)) SFileBlock;


typedef struct _PACKET_TIME_COMM
{
  ULONG len;
  UCHAR type;
  SCommand comm;
  CLOCKDATA cd;
  USHORT crc;
} __attribute__ ((packed)) PACKET_TIME_COMM;

typedef struct _PACKET_SET_CONTRACT_STATUS_CMD
{
  ULONG len;
  UCHAR type;
  SCommand comm;
  SContractStatusBlock cntrState;
  USHORT crc;
} __attribute__ ((packed)) PACKET_SET_CONTRACT_STATUS_CMD;

typedef struct _PACKET_SET_CONTRACT_RANGE_CMD
{
  ULONG len;
  UCHAR type;
  SCommand comm;
  SContractRangeBlock cntrRangeBlock;
  USHORT crc;
} __attribute__ ((packed)) PACKET_SET_CONTRACT_RANGE_CMD;

typedef struct _PACKET_SET_UPDATE_FILE
{
  ULONG len;
  UCHAR type;
  SCommand comm;
  SFileBlock fileBlock;
  USHORT crc;
} __attribute__ ((packed)) PACKET_SET_UPDATE_FILE;

#endif		// struc_h
