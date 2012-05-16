#ifndef ADBK_STRUCT_H
#define ADBK_STRUCT_H

#define ADBK_PORT 1024

#include <common_struct.h>

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

struct __attribute__ ((packed)) SCommand
{
  char  	cmd;            //команда
  unsigned long event;          //№ события
  int		count;          //кол-во (- предыдущие, +последующие)
  char  	block_type;     //тип блока
  int   	block_count;    //кол-во передаваемых блоков
};

struct __attribute__ ((packed)) SAnswer
{
  char		err_code;    	//код ошибки
  char		block_type;  	//тип блока
  int		block_count; 	//кол-во возвращаемых блоков
};

struct __attribute__ ((packed)) SBillAcceptor	// 1 + 1 + 4 = 6
{
  bool  enabled;
  char  err_code;
  unsigned int money;
};

struct __attribute__ ((packed)) SCardReader		// 1 + 1 + 8 + 8 = 18
{
  bool  enabled;
  char  err_code;
  unsigned char hw_ver[8];
  unsigned char sn[8];
};

struct __attribute__ ((packed)) SVideoSystem	// 1 + 1 = 2
{
  bool  enabled;
  char  err_code;
};

struct __attribute__ ((packed)) SPrinter		// 1 + 1 + 4 = 6
{
  bool  enabled;
  char  err_code;
  int   check_remain;
};

struct __attribute__ ((packed)) SStateBlock		// 32 + 10 + 1 + 4 + 1
{
  SBillAcceptor bill;		// 6
  SCardReader   card;		// 18
  SVideoSystem  video;		// 2
  SPrinter      printer;	// 6
  unsigned char sw_ver[10];
  char state;	// текущее состояние см. const stXXXXXX
};

struct __attribute__ ((packed)) SFileBlock
{
    char filename[255];
    char md5[32];
};

#endif // ADBK_STRUCT_H
