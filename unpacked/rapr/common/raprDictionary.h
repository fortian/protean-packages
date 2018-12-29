#include "map"
#include "vector"
#include "raprNumberGenerator.h"
#ifndef _RAPR_DICTIONARY
#define _RAPR_DICTIONARY

#include "raprPayload.h"

typedef _STL::vector<char *> RaprStringVector;
typedef _STL::map<char *,RaprStringVector *,ltstr> RaprDictionaryMap;
typedef _STL::multimap<int, char *> RaprDictionaryNameSpaceMap;
typedef _STL::map<char *,RaprDictionaryMap *,ltstr> RaprDictionaryNameSpace;

class RaprDictionaryTransfer {
	public:
	  RaprDictionaryTransfer();
	  ~RaprDictionaryTransfer();
	  void SetPRNG(RaprPRNG *inPrng) {prng = inPrng;}
	  RaprPRNG *GetPRNG() {return prng;}
	  unsigned int GetRandom();
	  float GetRandomF();
	  char *GetSrcIP() { return srcip; }
	  char *GetSrcPort() { return srcport; }
	  char *GetDstIP() { return dstip; }
	  char *GetDstPort() { return dstport; }
	  RaprPayload *GetPayload() { return payload; }
	  void SetSrcIP(char *inVal) { srcip = inVal; }
	  void SetSrcPort(char *inVal) { srcport = inVal; }
	  void SetDstIP(char *inVal) { dstip = inVal; }
	  void SetDstPort(char *inVal) { dstport = inVal; }
	  void SetPayload(RaprPayload *inPayload) { payload = inPayload; }
      void operator=(RaprDictionaryTransfer* trans);
  private:
	  RaprPRNG *prng;
	  char *srcip;
	  char *srcport;
	  char *dstip;
	  char *dstport;
	  RaprPayload *payload;
};

class RaprDictionary {
   public:
	   RaprDictionary();
	   RaprDictionary(char *inFile);
	   ~RaprDictionary();
	   RaprStringVector *translate(char *baseString,RaprDictionaryTransfer *trans);
	   RaprStringVector *translate(char *baseString);
	   void SetPersonality(char *inFileName) {;}
	   void SetDefinition(char *inFileName) {;}
	   void SetValue(char *inNS,char *inField,char *inVal);
	   void ResetValue(char *inNS,char *inField,char *inVal);
	   void SetValue(char *inField,char *inVal) { SetValue("DEFAULT",inField,inVal); }
	   void ResetValue(char *inField,char *inVal) { ResetValue("DEFAULT",inField,inVal); }
	   bool LoadFile(char *inFile);
 private:
	   int count(char *inString,char inSep);
	   int location(char *inString,char inSep);
	   char *newString(int inSize);
	   RaprStringVector *lookup(char *inIndex,RaprDictionaryTransfer *trans);
	   RaprStringVector *namevalue(char *inNS,char *inVal,RaprDictionaryTransfer *trans,char *arg);
	   RaprStringVector *PacketNameSpace(char *inVal,RaprDictionaryTransfer *trans);
	   RaprStringVector *SystemNameSpace(char *inVal,RaprDictionaryTransfer *trans,char *arg);
	   RaprStringVector *ParseNestedField(char *buf,int z,RaprDictionaryTransfer *trans);
	   RaprStringVector *ParseNestedField(char *buf,char *name,RaprDictionaryTransfer *trans);

	   RaprDictionaryNameSpaceMap nsm;
	   RaprDictionaryNameSpace dmap;
	   int mapcount;
};

#endif //_RAPR_DICTIONARY

