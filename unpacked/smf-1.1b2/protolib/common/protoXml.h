#ifndef PROTO_XML_NODE_H
#define PROTO_XML_NODE_H

#include <string>
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include <math.h>

// This code is a work in progress and may not be that useful yet.  TinyXML might
// be a better alternative here?


using namespace std;


/*! \brief Create an instance of this class, and use CreateTree() or CreateTreeFromFile() to parse XML data.
 * Then use ProtoXmlTree::iterator to access extracted information. Use ProtoXmlTree::iterator with
 * the access methods of ProtoXmlNode to modify the information, and CreateXMLString() to produce a
 * well-formed XML document from the modified data.
 */
class ProtoXmlTree
{
	public:

		class Iterator;
		class ProtoXmlNode;

		// ctors/dtor
		ProtoXmlTree() :	currentNode(NULL), rootNode(NULL), lastCharData(""), lastName(""),
							lastChar('\0'), lastAttValue(""), lastError(""), lastReference("") {}
		~ProtoXmlTree() {if(rootNode) delete rootNode;}
        
		bool CreateTree(string s);						
		bool CreateTreeFromFile(string filePath);		

		string CreateXMLString(int level = 0);

		// access methods
		string GetLastError(void) {return lastError;}	//!< Access method to retrieve an error message on failure.
		string GetXMLString() {return xmlString;}		//!< \brief Access method to retrieve the output of CreateXMLString().

		// Iterator methods
		Iterator begin();   //!< Analogous to the STL class' begin()
		Iterator end()      //!< Analogous to the STL class' end()
            {return Iterator(NULL);}	

	    /*! \brief Analogous to the STL class' iterators. Used to access data extracted from an XML document after parsing it with
		 * CreateTree() or CreateTreeFromFile().
		 *
		 * Iterate between ProtoXmlTree::begin() and ProtoXmlTree::end(). To access data, 
         * dereference the iterator. For example, (*itr).GetTagName() returns the tag 
         * name (if any) of the current node.
		 */
		class Iterator
		{
			friend class ProtoXmlTree;

			public:
                Iterator() : currentNode(NULL) {}
				Iterator(ProtoXmlNode *startNode) {currentNode = startNode;}
				~Iterator() {}

				ProtoXmlNode& operator*() //!< Returns the ProtoXmlNode being pointed to.
                    {return *currentNode;}	
				bool operator==(Iterator right) 
                    {return (currentNode == right.currentNode);}
				bool operator!=(Iterator right)
                    {return (currentNode != right.currentNode);}
				Iterator operator++();	  //!< Prefix operator; iterate one step in a depth-first search.
				Iterator operator++(int)  //!< Postfix operator; iterate one step in a depth-first search.
                    {Iterator temp(*this); operator++(); return temp;} 

			private:
				
				ProtoXmlNode *currentNode;	//!< Keeps track of the current node.
				list<ProtoXmlNode *> open;	//!< Keeps track of unvisited nodes while iterating through a depth-first search.
		};  // end class ProtoXmlTree::Iterator()

		//! Used by ProtoXmlTree to store information that is extracted during parsing.
		class ProtoXmlNode
		{
			friend class ProtoXmlTree;
			friend class Iterator;

			public:

				ProtoXmlNode() : parent(NULL), charData(""), tagName(""), level(0) {}
				~ProtoXmlNode();

				// member access functions
				void AddChild(ProtoXmlNode *child) //!< Links a new child node and sets it parent. 
                {
                    children.push_back(child); 
                    child->parent = this;
                } 
				ProtoXmlNode *GetParent() 
                    {return parent;}
				void SetCharData(const string &charDataIn) //!< Allows modification of character data.
                    {charData = charDataIn;} 
				string GetCharData() 
                    {return charData;}
				void SetTagName(const string &tagNameIn) 
                    {tagName = tagNameIn;}		//!< Allows modification of tag name.
				string GetTagName() 
                    {return tagName;}
				void SetAttribute(const string &attName, const string &attValue) 
                    {attributes[attName] = attValue;} //!< Allows modification of attributes.
				string GetAttribute(const string &attName) 
                    {return attributes[attName];}
                
               unsigned int GetLevel() {return level;}

			private:
                void SetLevel(unsigned int theLevel) {level = theLevel;}
                ProtoXmlNode *parent;				//!< Pointer to the parent node.
				vector<ProtoXmlNode *> children;	//!< Array of pointers to child nodes.
				map<string, string> attributes;		//!< Stores any parsed attribute data associated with the node. Use GetAttribute() to retrieve it.
				string charData;					//!< Stores any parsed character data associated with the node. Use GetCharData() to retrieve it.
				string tagName;						//!< Stores any parsed tag name associated with the node. Use GetTagName() to retrieve it.
	            unsigned int level;
        };

	private:

		// parsing functions
		bool ParseAttDef(string::iterator &itr);
		bool ParseAttlistDecl(string::iterator &itr);
		bool ParseAttribute(string::iterator &itr);
		bool ParseAttType(string::iterator &itr);
		bool ParseAttValue(string::iterator &itr);
		bool ParseBaseChar(string::iterator &itr);
		bool ParseCData(string::iterator &itr);
		bool ParseCDEnd(string::iterator &itr);
		bool ParseCDSect(string::iterator &itr);
		bool ParseCDStart(string::iterator &itr);
		bool ParseChar(string::iterator &itr);
		bool ParseChar(string::iterator &itr, const char c);
		bool ParseCharData(string::iterator &itr);
		bool ParseCharRef(string::iterator &itr);
		bool ParseChildren(string::iterator &itr);
		bool ParseChoice(string::iterator &itr);
		bool ParseCombiningChar(string::iterator &itr);
		bool ParseComment(string::iterator &itr);
		bool ParseContent(string::iterator &itr);
		bool ParseContentspec(string::iterator &itr);
		bool ParseCp(string::iterator &itr);
		bool ParseDeclSep(string::iterator &itr);
		bool ParseDefaultDecl(string::iterator &itr);
		bool ParseDigit(string::iterator &itr);
		bool ParseDoctypedecl(string::iterator &itr);
		bool ParseDocument(string::iterator &itr);
		bool ParseElement(string::iterator &itr);
		bool ParseElementdecl(string::iterator &itr);
		bool ParseEmptyElemTag(string::iterator &itr);
		bool ParseEncName(string::iterator &itr);
		bool ParseEncodingDecl(string::iterator &itr);
		bool ParseEntityDecl(string::iterator &itr);
		bool ParseEntityDef(string::iterator &itr);
		bool ParseEntityRef(string::iterator &itr);
		bool ParseEntityValue(string::iterator &itr);
		bool ParseEnumeratedType(string::iterator &itr);
		bool ParseEnumeration(string::iterator &itr);
		bool ParseEq(string::iterator &itr);
		bool ParseETag(string::iterator &itr);
		bool ParseExtender(string::iterator &itr);
		bool ParseExternalID(string::iterator &itr);
		bool ParseGEDecl(string::iterator &itr);
		bool ParseIdeographic(string::iterator &itr);
		bool ParseIntSubset(string::iterator &itr);
		bool ParseLetter(string::iterator &itr);
		bool ParseMarkupdecl(string::iterator &itr);
		bool ParseMisc(string::iterator &itr);
		bool ParseMixed(string::iterator &itr);
		bool ParseName(string::iterator &itr);
		bool ParseNameChar(string::iterator &itr);
		bool ParseNDataDecl(string::iterator &itr);
		bool ParseNmtoken(string::iterator &itr);
		bool ParseNotationDecl(string::iterator &itr);
		bool ParseNotationType(string::iterator &itr);
		bool ParsePEDecl(string::iterator &itr);
		bool ParsePEDef(string::iterator &itr);
		bool ParsePEReference(string::iterator &itr);
		bool ParsePI(string::iterator &itr);
		bool ParsePITarget(string::iterator &itr);
		bool ParseProlog(string::iterator &itr);
		bool ParsePubidChar(string::iterator &itr);
		bool ParsePubidLiteral(string::iterator &itr);
		bool ParsePublicID(string::iterator &itr);
		bool ParseReference(string::iterator &itr);
		bool ParseS(string::iterator &itr);
		bool ParseSDDecl(string::iterator &itr);
		bool ParseSeq(string::iterator &itr);
		bool ParseSTag(string::iterator &itr);
		bool ParseString(string::iterator &itr, string s);
		bool ParseStringType(string::iterator &itr);
		bool ParseSystemLiteral(string::iterator &itr);
		bool ParseTokenizedType(string::iterator &itr);
		bool ParseVersionInfo(string::iterator &itr);
		bool ParseVersionNum(string::iterator &itr);
		bool ParseXMLDecl(string::iterator &itr);

		ProtoXmlNode *rootNode;		//!< Pointer to the root of the output tree.

		/*! \addtogroup parsing
		 * Relevant data are stored in the parsing variables during parsing.
		 *
		 * @{
		 */
		string xmlString;			//!< The entire XML document to be parsed.
		ProtoXmlNode *currentNode;	//!< \brief Keeps track of which node in the output tree is currently being constructed
		string lastCharData;		/*!< \brief Stores whatever character data was last successfully parsed.
									 *		\sa ParseCharData()
									 */
		list<string> tagNames;		/*!< \brief A stack of tag names to ensure they are encountered in parenthetical order.
									 *		\sa ParseSTag(), ParseETag()
									 */
		string lastName;			/*!< \brief Stores whichever name was last successfully parsed.
									 *		\sa ParseName()
									 */
		wchar_t lastChar;			/*!< \brief Stores whichever character was last successfully parsed.
									 *		\sa ParseChar()
									 */
		string lastAttValue;		/*!< \brief Stores whichever attribute value was last successfully parsed.
									 *		\sa ParseAttValue()
									 */
		string lastError;			/*!< \brief Stores an error message on failure to parse.
									 *		\sa GetLastError()
									 */
		string lastReference;		/*!< \brief Stores whichever reference was last successfully parsed.
									 *		\sa ParseReference()
									 */
		/*! @} */
};

#endif
