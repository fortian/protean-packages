#include "protoXml.h"

using namespace std;

/*! \brief Parses a string of XML data and creates an output tree.
 *
 * \param s The string representing a well-formed XML document.
 */
bool ProtoXmlTree::CreateTree(string s)
{
	if(rootNode != NULL)
		delete rootNode;

	xmlString = s;
	string::iterator itr = xmlString.begin();

	// not a well-formed string
	if(ParseDocument(itr) == false)
	{
		if(rootNode != NULL)
		{
			delete rootNode;
			rootNode = NULL;
		}
		return false;
	}
	return true;
} // ProtoXmlTree::CreateTree()


/*! \brief Parses a file of XML data and creates an output tree.
 *
 * After parsing, use ProtoXmlTree::iterator to access data.
 *
 * \param filePath The complete path string to the file containing XML data.
 */
bool ProtoXmlTree::CreateTreeFromFile(string filePath)
{
	string line;
	ifstream xmlFile(filePath.c_str());
	if(xmlFile.is_open() != true)
	{
		lastError = "ERROR: could not read file \"" + filePath + "\"\n";
		return false;
	}
	while(getline(xmlFile, line))
		xmlString += line + "\n";
	
	if(rootNode != NULL)
		delete rootNode;

	string::iterator itr = xmlString.begin();

	// not a well-formed string
	if(ParseDocument(itr) == false)
	{
		if(rootNode != NULL)
		{
			delete rootNode;
			rootNode = NULL;
		}
		return false;
	}
	return true;
} // ProtoXmlTree::CreateTreeFromFile()


//! \brief Produces a well-formed XML string from the currently stored tree.
string ProtoXmlTree::CreateXMLString(int level)
{
	if(level == 0)
	{
		xmlString = "";
		currentNode = rootNode;
	}
	// add opening tag
	if(currentNode->tagName.empty() != true)
	{
		// indent
		xmlString += "\n";
		for(int i = 0; i < level; i++)
			xmlString += "\t";

		xmlString += "<" + currentNode->tagName;
			// add attributes
			for(map<string, string>::iterator i = currentNode->attributes.begin(); i != currentNode->attributes.end(); i++)
				xmlString += " " + (*i).first + "='" + (*i).second + "'";
		xmlString += ">";
	}
	// add char data - ignore whitespace nodes
	bool whiteSpaceOnly = true;
	for(unsigned int i = 0; i < currentNode->charData.length(); i++)
	{
		char c = currentNode->charData[i];
		if(c != '\x20' && c != '\x9' && c != '\xA' && c != '\xD')
		{
			whiteSpaceOnly = false;
			break;
		}
	}
	if(whiteSpaceOnly != true)
	{
		// indent
		xmlString += "\n";
		for(int i = 0; i < level; i++)
			xmlString += "\t";

		xmlString += currentNode->charData;
	}
	// recurse to children
	for(unsigned int i = 0; i < currentNode->children.size(); i++)
	{
		currentNode = currentNode->children[i];
		CreateXMLString(level + 1);
	}
	// add end tag
	if(currentNode->tagName.empty() != true)
	{
		// indent
		xmlString += "\n";
		for(int i = 0; i < level; i++)
			xmlString += "\t";

		xmlString += "</" + currentNode->tagName + ">";
	}

	if(level == 0)
    {
		return xmlString;
    }
	else
	{
		currentNode = currentNode->parent;
		return "";
	}
} // ProtoXmlTree::CreateXMLString()


// ProtoXmlTree::begin()
ProtoXmlTree::Iterator ProtoXmlTree::begin()
{
	ProtoXmlTree::Iterator itr(rootNode);
	if(rootNode == NULL)
		return itr;

	for(int i = (int)rootNode->children.size() - 1; i >= 0; i--)
		itr.open.push_front(rootNode->children[i]);

	return itr;
} // ProtoXmlTree::begin()


// ProtoXmlTree::iterator::operator++
// Returns the next node in a depth-first traversal of the tree.
ProtoXmlTree::Iterator ProtoXmlTree::Iterator::operator++ ()
{
	// is the search complete?
	if(open.empty() == true)
	{
		currentNode = NULL;
		return *this;
	}

	// move to first child in open list and add new children
	currentNode = open.front();
	open.pop_front();
	for(int i = (int)currentNode->children.size() - 1; i >= 0; i--)
		open.push_front(currentNode->children[i]);

	return *this;
} // ProtoXmlTree::iterator::operator++


// ProtoXmlNode::~ProtoXmlNode()
ProtoXmlTree::ProtoXmlNode::~ProtoXmlNode()
{
	// unlink from parent
	if(parent != NULL)
	{
		vector<ProtoXmlNode *>::iterator i;
		for(i = parent->children.begin(); *i != this && i != parent->children.end(); i++) {}
		parent->children.erase(i);
		parent = NULL;
	}
	
	// delete children
	if(!children.empty())
		for(unsigned int i = 0; i < children.size(); i++)
			delete children[i];
} // ProtoXmlNode::~ProtoXmlNode()





















///////////////////////
// Parsing functions //
///////////////////////

/*! \defgroup parsing Functions and variables used during parsing
 * These functions simply follow the productions found in the XML 1.0 specification.
 * Each function takes an std::string::iterator as a parameter, which is used to keep track
 * of the current position in the input string.
 * Any functions they are called from are listed in the "See also:" section.
 * @{
 */

/*! \brief AttDef ::= S Name S AttType S DefaultDecl
 * 
 * \sa ParseAttlistDecl()
 */
bool ProtoXmlTree::ParseAttDef(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseS(itr) && ParseName(itr) && ParseS(itr) && ParseAttType(itr) && ParseS(itr) && ParseDefaultDecl(itr))
		return true;
	itr = itrSave;
	return false;
}



/*! \brief AttlistDecl ::= '<!ATTLIST' S Name AttDef* S? '>' 
*
* \sa ParseMarkupdecl()
*/
bool ProtoXmlTree::ParseAttlistDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "<!ATTLIST") && ParseS(itr) && ParseName(itr))
	{
		while(ParseAttDef(itr)) {}
		ParseS(itr);
		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief Attribute ::= Name Eq AttValue
 *
 * \sa ParseEmptyElemTag(), ParseSTag()
 */ 
bool ProtoXmlTree::ParseAttribute(string::iterator &itr)
{
	//printf("ParseAttribute: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseName(itr) && ParseEq(itr) && ParseAttValue(itr))
	{
		currentNode->attributes[lastName] = lastAttValue;
		return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief AttType ::= StringType | TokenizedType | EnumeratedType 
 * 
 * \sa ParseAttDef()
 */
bool ProtoXmlTree::ParseAttType(string::iterator &itr)
{
	return ParseStringType(itr) || ParseTokenizedType(itr) || ParseEnumeratedType(itr);
}

/*! \brief AttValue ::=	'"' ([^<&"] | Reference)* '"' | "'" ([^<&'] | Reference)* "'"
 *
 * \sa ParseAttribute(), ParseDefaultDecl()
 */				
bool ProtoXmlTree::ParseAttValue(string::iterator &itr)
{
	//printf("ParseAttValue: %c\n", *itr);

	lastAttValue = "";

	// '"' ([^<&"] | Reference)* '"'
	string::iterator itrSave = itr;
	if(ParseChar(itr, '"'))
	{
		while(true)
		{
			if(*itr != '<' && *itr != '&' && *itr != '"')
				lastAttValue += *(itr++);
			else
			{
				ParseReference(itr);
				if(lastReference.empty() != false)
					break;
			}
		}
		if(ParseChar(itr, '"'))
			return true;
	}

	// "'" ([^<&'] | Reference)* "'"
	itr = itrSave;
	if(ParseChar(itr, '\''))
	{
		while(true)
		{
			if(*itr != '<' && *itr != '&' && *itr != '\'' )
				lastAttValue += *(itr++);
			else
			{
				ParseReference(itr);
				if(lastReference.empty() != false)
					break;
			}
		}
		if(ParseChar(itr, '\''))
			return true;
	}
	itr = itrSave;
	lastAttValue = "";
	return false;
}

/*! \brief BaseChar ::=	(large table)
 *
 * See XML 1.0 specification for the full list.
 *
 * \sa ParseLetter()
 */
bool ProtoXmlTree::ParseBaseChar(string::iterator &itr)
{
	//printf("ParseBaseChar: %c\n", *itr);

	// BaseCharTable[i][0] = beginning of range
	// BaseCharTable[i][1] = end of range
	// if single character, BaseCharTable[i][1] = BaseCharTable[i][0]
	static const wchar_t BaseCharTable[202][2] = 
    {
		L'\x0041', L'\x005A', L'\x0061', L'\x007A', L'\x00C0', L'\x00D6', L'\x00D8', L'\x00F6', L'\x00F8', L'\x00FF',
		L'\x0100', L'\x0131', L'\x0134', L'\x013E', L'\x0141', L'\x0148', L'\x014A', L'\x017E', L'\x0180', L'\x01C3',
		L'\x01CD', L'\x01F0', L'\x01F4', L'\x01F5', L'\x01FA', L'\x0217', L'\x0250', L'\x02A8', L'\x02BB', L'\x02C1',
		L'\x0386', L'\x0386', L'\x0388', L'\x038A', L'\x038C', L'\x038C', L'\x038E', L'\x03A1', L'\x03A3', L'\x03CE',
		L'\x03D0', L'\x03D6', L'\x03DA', L'\x03DA', L'\x03DC', L'\x03DC', L'\x03DE', L'\x03DE', L'\x03E0', L'\x03E0',
		L'\x03E2', L'\x03F3', L'\x0401', L'\x040C', L'\x040E', L'\x044F', L'\x0451', L'\x045C', L'\x045E', L'\x0481',
		L'\x0490', L'\x04C4', L'\x04C7', L'\x04C8', L'\x04CB', L'\x04CC', L'\x04D0', L'\x04EB', L'\x04EE', L'\x04F5',
		L'\x04F8', L'\x04F9', L'\x0531', L'\x0556', L'\x0559', L'\x0559', L'\x0561', L'\x0586', L'\x05D0', L'\x05EA',
		L'\x05F0', L'\x05F2', L'\x0621', L'\x063A', L'\x0641', L'\x064A', L'\x0671', L'\x06B7', L'\x06BA', L'\x06BE',
		L'\x06C0', L'\x06CE', L'\x06D0', L'\x06D3', L'\x06D5', L'\x06D5', L'\x06E5', L'\x06E6', L'\x0905', L'\x0939',
		L'\x093D', L'\x093D', L'\x0958', L'\x0961', L'\x0985', L'\x098C', L'\x098F', L'\x0990', L'\x0993', L'\x09A8',
		L'\x09AA', L'\x09B0', L'\x09B2', L'\x09B2', L'\x09B6', L'\x09B9', L'\x09DC', L'\x09DD', L'\x09DF', L'\x09E1',
		L'\x09F0', L'\x09F1', L'\x0A05', L'\x0A0A', L'\x0A0F', L'\x0A10', L'\x0A13', L'\x0A28', L'\x0A2A', L'\x0A30',
		L'\x0A32', L'\x0A33', L'\x0A35', L'\x0A36', L'\x0A38', L'\x0A39', L'\x0A59', L'\x0A5C', L'\x0A5E', L'\x0A5E',
		L'\x0A72', L'\x0A74', L'\x0A85', L'\x0A8B', L'\x0A8D', L'\x0A8D', L'\x0A8F', L'\x0A91', L'\x0A93', L'\x0AA8',
		L'\x0AAA', L'\x0AB0', L'\x0AB2', L'\x0AB3', L'\x0AB5', L'\x0AB9', L'\x0ABD', L'\x0ABD', L'\x0AE0', L'\x0AE0',
		L'\x0B05', L'\x0B0C', L'\x0B0F', L'\x0B10', L'\x0B13', L'\x0B28', L'\x0B2A', L'\x0B30', L'\x0B32', L'\x0B33',
		L'\x0B36', L'\x0B39', L'\x0B3D', L'\x0B3D', L'\x0B5C', L'\x0B5D', L'\x0B5F', L'\x0B61', L'\x0B85', L'\x0B8A',
		L'\x0B8E', L'\x0B90', L'\x0B92', L'\x0B95', L'\x0B99', L'\x0B9A', L'\x0B9C', L'\x0B9C', L'\x0B9E', L'\x0B9F',
		L'\x0BA3', L'\x0BA4', L'\x0BA8', L'\x0BAA', L'\x0BAE', L'\x0BB5', L'\x0BB7', L'\x0BB9', L'\x0C05', L'\x0C0C',
		L'\x0C0E', L'\x0C10', L'\x0C12', L'\x0C28', L'\x0C2A', L'\x0C33', L'\x0C35', L'\x0C39', L'\x0C60', L'\x0C61',
		L'\x0C85', L'\x0C8C', L'\x0C8E', L'\x0C90', L'\x0C92', L'\x0CA8', L'\x0CAA', L'\x0CB3', L'\x0CB5', L'\x0CB9',
		L'\x0CDE', L'\x0CDE', L'\x0CE0', L'\x0CE1', L'\x0D05', L'\x0D0C', L'\x0D0E', L'\x0D10', L'\x0D12', L'\x0D28',
		L'\x0D2A', L'\x0D39', L'\x0D60', L'\x0D61', L'\x0E01', L'\x0E2E', L'\x0E30', L'\x0E30', L'\x0E32', L'\x0E33',
		L'\x0E40', L'\x0E45', L'\x0E81', L'\x0E82', L'\x0E84', L'\x0E84', L'\x0E87', L'\x0E88', L'\x0E8A', L'\x0E8A',
		L'\x0E8D', L'\x0E8D', L'\x0E94', L'\x0E97', L'\x0E99', L'\x0E9F', L'\x0EA1', L'\x0EA3', L'\x0EA5', L'\x0EA5',
		L'\x0EA7', L'\x0EA7', L'\x0EAA', L'\x0EAB', L'\x0EAD', L'\x0EAE', L'\x0EB0', L'\x0EB0', L'\x0EB2', L'\x0EB3',
		L'\x0EBD', L'\x0EBD', L'\x0EC0', L'\x0EC4', L'\x0F40', L'\x0F47', L'\x0F49', L'\x0F69', L'\x10A0', L'\x10C5',
		L'\x10D0', L'\x10F6', L'\x1100', L'\x1100', L'\x1102', L'\x1103', L'\x1105', L'\x1107', L'\x1109', L'\x1109',
		L'\x110B', L'\x110C', L'\x110E', L'\x1112', L'\x113C', L'\x113C', L'\x113E', L'\x113E', L'\x1140', L'\x1140',
		L'\x114C', L'\x114C', L'\x114E', L'\x114E', L'\x1150', L'\x1150', L'\x1154', L'\x1155', L'\x1159', L'\x1159',
		L'\x115F', L'\x1161', L'\x1163', L'\x1163', L'\x1165', L'\x1165', L'\x1167', L'\x1167', L'\x1169', L'\x1169',
		L'\x116D', L'\x116E', L'\x1172', L'\x1173', L'\x1175', L'\x1175', L'\x119E', L'\x119E', L'\x11A8', L'\x11A8',
		L'\x11AB', L'\x11AB', L'\x11AE', L'\x11AF', L'\x11B7', L'\x11B8', L'\x11BA', L'\x11BA', L'\x11BC', L'\x11C2',
		L'\x11EB', L'\x11EB', L'\x11F0', L'\x11F0', L'\x11F9', L'\x11F9', L'\x1E00', L'\x1E9B', L'\x1EA0', L'\x1EF9',
		L'\x1F00', L'\x1F15', L'\x1F18', L'\x1F1D', L'\x1F20', L'\x1F45', L'\x1F48', L'\x1F4D', L'\x1F50', L'\x1F57',
		L'\x1F59', L'\x1F59', L'\x1F5B', L'\x1F5B', L'\x1F5D', L'\x1F5D', L'\x1F5F', L'\x1F7D', L'\x1F80', L'\x1FB4',
		L'\x1FB6', L'\x1FBC', L'\x1FBE', L'\x1FBE', L'\x1FC2', L'\x1FC4', L'\x1FC6', L'\x1FCC', L'\x1FD0', L'\x1FD3',
		L'\x1FD6', L'\x1FDB', L'\x1FE0', L'\x1FEC', L'\x1FF2', L'\x1FF4', L'\x1FF6', L'\x1FFC', L'\x2126', L'\x2126',
		L'\x212A', L'\x212B', L'\x212E', L'\x212E', L'\x2180', L'\x2182', L'\x3041', L'\x3094', L'\x30A1', L'\x30FA',
		L'\x3105', L'\x312C', L'\xAC00', L'\xD7A3'
	};

	// binary search the table
	int left = 0, right = 201;
	while(left <= right)
	{
		int middle = (int)floor((left + right) / 2.0);
		//printf("middle: %d\n", middle);
		if(*itr >= BaseCharTable[middle][0] && *itr <= BaseCharTable[middle][1])
		{
			lastChar = *(itr++);
			return true;
		}
		if(*itr < BaseCharTable[middle][0])
			right = middle - 1;
		else
			left = middle + 1;
	}
	return false;
}

/*! \brief CData ::= (Char* - (Char* ']]>' Char*))
 * 
 * \sa ParseCDSect()
 */ 
bool ProtoXmlTree::ParseCData(string::iterator &itr)
{
	//printf("ParseCData: %c\n", *itr);

	while(!(*itr == ']' && *(itr + 1) == ']' && *(itr + 2) == '>') && ParseChar(itr)) {}
	return true;
}

/*! \brief CDEnd ::= ']]>'
 *
 * \sa ParseCDSect()
 */	
bool ProtoXmlTree::ParseCDEnd(string::iterator &itr)
{
	//printf("ParseCDEnd: %c\n", *itr);

	return ParseString(itr, "]]>");
}

/*! \brief CDSect ::= CDStart CData CDEnd
 * 
 * \sa ParseContent()
 */
bool ProtoXmlTree::ParseCDSect(string::iterator &itr)
{
	//printf("ParseCDSect: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseCDStart(itr) && ParseCData(itr) && ParseCDEnd(itr))
		return true;
	itr = itrSave;
	return false;
}

/*! \brief CDStart ::= '<![CDATA['
 *
 * \sa ParseCDSect()
 */
bool ProtoXmlTree::ParseCDStart(string::iterator& itr)
{
	//printf("ParseCDStart: %c\n", *itr);

	return ParseString(itr, "<![CDATA[");
}

/*! \brief Char ::= (table)
 *
 * See XML 1.0 specification for the full list.
 *
 * \sa ParseCData(), ParseComment(), ParsePI()
 */
// 
bool ProtoXmlTree::ParseChar(string::iterator& itr)
{
	//printf("ParseChar: %c\n", *itr);

	return	((*itr == '\x9') || 
             (*itr == '\xA') || 
             (*itr == '\xD') || 
             ((*itr >= '\x20')));// &&   // (TBD) look at this more
             // (*itr <= L'\xD7FF')) ||
			 //((*itr >= L'\xE000') && 
             // (*itr <= L'\xFFFD')));// || 
             //(*itr >= '\uD800\uDC00' && *itr <= '\uDBFF\uDFFF'));
}

/*! \brief Utility function for parsing one character.
 *
 * If the next input character matches, it is consumed and true is returned; otherwise, nothing
 * is consumed and false is returned.
 *
 * \param c Character to be tested against input
 */
bool ProtoXmlTree::ParseChar(string::iterator &itr, const char c)
{
	if(*itr == c)
	{
		lastChar = *(itr++);
		return true;
	}
	return false;
}

/*! \brief CharData ::= [^<&]* - ([^<&]* ']]>' [^<&]*)
 *
 * \sa ParseContent()
 */
bool ProtoXmlTree::ParseCharData(string::iterator &itr)
{
	//printf("ParseCharData: %c\n", *itr);

	lastCharData = "";

	// consume matching input, and add it to lastCharData
	while(*itr != '<' && *itr != '&' && !(*itr == ']' && *(itr + 1) == ']' && *(itr + 2) == '>'))
		lastCharData += *(itr++);
	return true;
}

/*! \brief CharRef ::=	\verbatim'&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'\endverbatim
 *
 * This will have to be modified to implement reference inclusion.
 *
 * \sa ParseReference()
 */
bool ProtoXmlTree::ParseCharRef(string::iterator &itr)
{
	//printf("ParseCharRef: %c\n", *itr);

	// '&#' [0-9]+ ';'
	string::iterator itrSave = itr;
	if(ParseString(itr, "&#") && *itr >= '0' && *itr <= '9')
	{
		string charRefValue = "";
		do {charRefValue += *(itr++);}
			while(*itr >= '0' && *itr <= '9');

		if(ParseChar(itr, ';'))
		{
			char charRef = static_cast<char>(atoi(charRefValue.c_str()));
			lastReference = charRef;
			return true;
		}
	}

	// '&#x' [0-9a-fA-F]+ ';'
	itr = itrSave;
	if(ParseString(itr, "&#x") && ((*itr >= '0' && *itr <= '9') || (*itr >= 'a' && *itr <= 'f') || (*itr >= 'A' && *itr <= 'F')))
	{
		string charRefValue = "";
		do {charRefValue += *(itr++);}
			while((*itr >= '0' && *itr <= '9') || (*itr >= 'a' && *itr <= 'f') || (*itr >= 'A' && *itr <= 'F'));
		if(ParseChar(itr, ';'))
		{
			char charRef = static_cast<char>(strtol(charRefValue.c_str(), NULL, 16));
			lastReference = charRef;
			return true;
		}
	}
	itr = itrSave;
	return false;
}

/*! \brief children ::= (choice | seq) ('?' | '*' | '+')?
 * 
 * \sa ParseContentspec()
 */
bool ProtoXmlTree::ParseChildren(string::iterator &itr)
{
	if(ParseChoice(itr) || ParseSeq(itr))
	{
		if(*itr == '?' || *itr == '*' || *itr == '+')
			itr++;
		return true;
	}
	return false;
}

/*! \brief choice ::= '(' S? cp ( S? '|' S? cp )+ S? ')'
 * 
 * \sa ParseChildren(), ParseCp()
 */
bool ProtoXmlTree::ParseChoice(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '('))
	{
		ParseS(itr);
		if(ParseCp(itr))
		{
			ParseS(itr);
			if(ParseChar(itr, '|'))
			{
				ParseS(itr);
				if(ParseCp(itr))
				{
					while(true)
					{
						string::iterator itrSave2 = itr;
						ParseS(itr);
						if(!ParseChar(itr, '|'))
						{
							itr = itrSave2;
							break;
						}
						ParseS(itr);
						if(!ParseCp(itr))
						{
							itr = itrSave2;
							break;
						}
					}
					ParseS(itr);
					if(ParseChar(itr, ')'))
						return true;
				}
			}
		}
	}
	itr = itrSave;
	return false;
}

/*! \brief CombiningChar ::= (large table)
 *
 * See XML 1.0 specification for the full list.
 *
 * \sa ParseNameChar()
 */
bool ProtoXmlTree::ParseCombiningChar(string::iterator &itr)
{
	//printf("ParseCombiningChar: %c\n", *itr);

	static const wchar_t CombiningCharTable[95][2] = {
		L'\x0300', L'\x0345', L'\x0360', L'\x0361', L'\x0483', L'\x0486', L'\x0591', L'\x05A1', L'\x05A3', L'\x05B9',
		L'\x05BB', L'\x05BD', L'\x05BF', L'\x05BF', L'\x05C1', L'\x05C2', L'\x05C4', L'\x05C4', L'\x064B', L'\x0652',
		L'\x0670', L'\x0670', L'\x06D6', L'\x06DC', L'\x06DD', L'\x06DF', L'\x06E0', L'\x06E4', L'\x06E7', L'\x06E8',
		L'\x06EA', L'\x06ED', L'\x0901', L'\x0903', L'\x093C', L'\x093C', L'\x093E', L'\x094C', L'\x094D', L'\x094D',
		L'\x0951', L'\x0954', L'\x0962', L'\x0963', L'\x0981', L'\x0983', L'\x09BC', L'\x09BC', L'\x09BE', L'\x09BE',
		L'\x09BF', L'\x09BF', L'\x09C0', L'\x09C4', L'\x09C7', L'\x09C8', L'\x09CB', L'\x09CD', L'\x09D7', L'\x09D7',
		L'\x09E2', L'\x09E3', L'\x0A02', L'\x0A02', L'\x0A3C', L'\x0A3C', L'\x0A3E', L'\x0A3E', L'\x0A3F', L'\x0A3F',
		L'\x0A40', L'\x0A42', L'\x0A47', L'\x0A48', L'\x0A4B', L'\x0A4D', L'\x0A70', L'\x0A71', L'\x0A81', L'\x0A83',
		L'\x0ABC', L'\x0ABC', L'\x0ABE', L'\x0AC5', L'\x0AC7', L'\x0AC9', L'\x0ACB', L'\x0ACD', L'\x0B01', L'\x0B03',
		L'\x0B3C', L'\x0B3C', L'\x0B3E', L'\x0B43', L'\x0B47', L'\x0B48', L'\x0B4B', L'\x0B4D', L'\x0B56', L'\x0B57',
		L'\x0B82', L'\x0B83', L'\x0BBE', L'\x0BC2', L'\x0BC6', L'\x0BC8', L'\x0BCA', L'\x0BCD', L'\x0BD7', L'\x0BD7',
		L'\x0C01', L'\x0C03', L'\x0C3E', L'\x0C44', L'\x0C46', L'\x0C48', L'\x0C4A', L'\x0C4D', L'\x0C55', L'\x0C56',
		L'\x0C82', L'\x0C83', L'\x0CBE', L'\x0CC4', L'\x0CC6', L'\x0CC8', L'\x0CCA', L'\x0CCD', L'\x0CD5', L'\x0CD6',
		L'\x0D02', L'\x0D03', L'\x0D3E', L'\x0D43', L'\x0D46', L'\x0D48', L'\x0D4A', L'\x0D4D', L'\x0D57', L'\x0D57',
		L'\x0E31', L'\x0E31', L'\x0E34', L'\x0E3A', L'\x0E47', L'\x0E4E', L'\x0EB1', L'\x0EB1', L'\x0EB4', L'\x0EB9',
		L'\x0EBB', L'\x0EBC', L'\x0EC8', L'\x0ECD', L'\x0F18', L'\x0F19', L'\x0F35', L'\x0F35', L'\x0F37', L'\x0F37',
		L'\x0F39', L'\x0F39', L'\x0F3E', L'\x0F3E', L'\x0F3F', L'\x0F3F', L'\x0F71', L'\x0F84', L'\x0F86', L'\x0F8B',
		L'\x0F90', L'\x0F95', L'\x0F97', L'\x0F97', L'\x0F99', L'\x0FAD', L'\x0FB1', L'\x0FB7', L'\x0FB9', L'\x0FB9',
		L'\x20D0', L'\x20DC', L'\x20E1', L'\x20E1', L'\x302A', L'\x302F', L'\x3099', L'\x3099', L'\x309A', L'\x309A'
	};

	// binary search the table
	int left = 0, right = 18;
	while(left <= right)
	{
		int middle = (int)floor((left + right) / 2.0);
		if(*itr >= CombiningCharTable[middle][0] && *itr <= CombiningCharTable[middle][1])
		{
			lastChar = *(itr++);
			return true;
		}
		if(*itr < CombiningCharTable[middle][0])
			right = middle - 1;
		else
			left = middle + 1;
	}
	return false;	
}

/*! \brief Comment ::=	'<!--' ((Char - '-') | ('-' (Char - '-')))* '-->'
 * 
 * \sa ParseContent(), ParseMarkupdecl(), ParseMisc()
 */
bool ProtoXmlTree::ParseComment(string::iterator &itr)
{
	//printf("ParseComment: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseString(itr, "<!--"))
	{
		while(!(*itr == '-' && *(itr + 1) == '-'))
			ParseChar(itr);
		if(ParseString(itr, "-->"))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief content ::= CharData? ((element | Reference | CDSect | PI | Comment) CharData?)*
 * 
 * \sa ParseElement()
 */
bool ProtoXmlTree::ParseContent(string::iterator &itr)
{
	//printf("ParseContent: %c\n", *itr);

	ParseCharData(itr);
	if(lastCharData.empty() != true) // found some character data
	{
		// give it its own node
		ProtoXmlNode *charDataNode = new ProtoXmlNode();
		charDataNode->charData = lastCharData;
        charDataNode->SetLevel(currentNode->GetLevel() + 1);
		currentNode->AddChild(charDataNode);
	}

	while(ParseElement(itr) || ParseReference(itr) || ParseCDSect(itr) || ParsePI(itr) || ParseComment(itr))
	{
		ParseCharData(itr);
		if(lastCharData.empty() != true) // found some character data
		{
			// give it its own node
			ProtoXmlNode *charDataNode = new ProtoXmlNode();
			charDataNode->charData = lastCharData;
            charDataNode->SetLevel(currentNode->GetLevel() + 1);
			currentNode->AddChild(charDataNode);
		}
	}
	return true;
}

/*! \brief contentspec ::= 'EMPTY' | 'ANY' | Mixed | children
 * 
 * \sa ParseElementdecl()
 */
bool ProtoXmlTree::ParseContentspec(string::iterator &itr)
{
	return ParseString(itr, "EMPTY") || ParseString(itr, "ANY") || ParseMixed(itr) || ParseChildren(itr);
}

/*! \brief cp ::= (Name | choice | seq) ('?' | '*' | '+')?
 *
 * \sa ParseChoice(), ParseSeq()
 */
bool ProtoXmlTree::ParseCp(string::iterator &itr)
{
	if(ParseName(itr) || ParseChoice(itr) || ParseSeq(itr))
	{
		if(*itr == '?' || *itr == '*' || *itr == '+')
			itr++;
		return true;
	}
	return false;
}

/*! \brief DeclSep ::= PEReference | S
 * 
 * \sa ParseIntSubset()
 */
bool ProtoXmlTree::ParseDeclSep(string::iterator &itr)
{
	return ParsePEReference(itr) || ParseS(itr);
}

/*! \brief DefaultDecl ::=	\verbatim'#REQUIRED' | '#IMPLIED' | (('#FIXED' S)? AttValue)\endverbatim
 *
 * \sa ParseAttDef()
 */
bool ProtoXmlTree::ParseDefaultDecl(string::iterator &itr)
{
	if(ParseString(itr, "#REQUIRED") || ParseString(itr, "#IMPLIED"))
		return true;
	string::iterator itrSave = itr;
	if(!(ParseString(itr, "#FIXED") && ParseS(itr)))
		itr = itrSave;
	if(ParseAttValue(itr))
		return true;
	itr = itrSave;
	return false;
}

/*! \brief doctypedecl ::= '<!DOCTYPE' S Name (S ExternalID)? S? ('[' intSubset ']' S?)? '>'
 * 
 * \sa ParseProlog()
 */
bool ProtoXmlTree::ParseDoctypedecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "<!DOCTYPE") && ParseS(itr) && ParseName(itr))
	{
		// (S ExternalID)?
		string::iterator itrSave2 = itr;
		if(!(ParseS(itr) && ParseExternalID(itr)))
			itr = itrSave2;
		ParseS(itr);
		// ('[' intSubset ']' S?)?
		itrSave2 = itr;
		if(ParseChar(itr, '[') && ParseIntSubset(itr) && ParseChar(itr, ']'))
			ParseS(itr);
		else
			itr = itrSave2;
		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief Digit ::= (table)
 *
 * See XML 1.0 specification for the full list.
 *
 * \sa ParseNameChar()
 */
bool ProtoXmlTree::ParseDigit(string::iterator &itr)
{
	//printf("ParseDigit: %c\n", *itr);

	static const wchar_t DigitTable[15][2] = {
		L'\x0030', L'\x0039', L'\x0660', L'\x0669', L'\x06F0', L'\x06F9', L'\x0966', L'\x096F', L'\x09E6', L'\x09EF',
		L'\x0A66', L'\x0A6F', L'\x0AE6', L'\x0AEF', L'\x0B66', L'\x0B6F', L'\x0BE7', L'\x0BEF', L'\x0C66', L'\x0C6F',
		L'\x0CE6', L'\x0CEF', L'\x0D66', L'\x0D6F', L'\x0E50', L'\x0E59', L'\x0ED0', L'\x0ED9', L'\x0F20', L'\x0F29'
	};

	// use binary search here since it's already written
	int left = 0, right = 14;
	while(left <= right)
	{
		int middle = (int)floor((left + right) / 2.0);
		if(*itr >= DigitTable[middle][0] && *itr <= DigitTable[middle][1])
		{
			lastChar = *(itr++);
			return true;
		}
		if(*itr < DigitTable[middle][0])
			right = middle - 1;
		else
			left = middle + 1;
	}
	return false;
}

/*! \brief document ::= prolog element Misc*
 * 
 * The starting symbol.
 */
bool ProtoXmlTree::ParseDocument(string::iterator &itr)
{
	//printf("ParseDocument: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseProlog(itr) && ParseElement(itr))
	{
		while(ParseMisc(itr)) {}
		return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief element ::= EmptyElemTag | STag content ETag
 *
 * \sa ParseContent(), ParseDocument()
 */	
bool ProtoXmlTree::ParseElement(string::iterator &itr)
{
	// create a new node to add parsed information to
	ProtoXmlNode *newNode = new ProtoXmlNode;
	if(rootNode == NULL)
    {
        newNode->SetLevel(0);
		rootNode = newNode;
    }
	else
    {
        newNode->SetLevel(currentNode->GetLevel() + 1);
		currentNode->AddChild(newNode);
    }
	currentNode = newNode;

	if(ParseEmptyElemTag(itr))
	{
		currentNode = currentNode->parent;
		return true;
	}

	// STag content ETag
	string::iterator itrSave = itr;
	if(ParseSTag(itr) && ParseContent(itr) && ParseETag(itr))
	{
		currentNode = currentNode->parent;
		return true;
	}

	// not an element; delete the new node
	currentNode = currentNode->parent;
	if(newNode == rootNode)
		rootNode = NULL;
	delete newNode;

	itr = itrSave;
	return false;
}

/*! \brief elementdecl ::= '<!ELEMENT' S Name S contentspec S? '>'
 * 
 * \sa ParseMarkupdecl()
 */
bool ProtoXmlTree::ParseElementdecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "<!ELEMENT") && ParseS(itr) && ParseName(itr) && ParseS(itr) && ParseContentspec(itr))
	{
		ParseS(itr);
		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'
 *
 * \sa ParseElement()
 */
bool ProtoXmlTree::ParseEmptyElemTag(string::iterator &itr)
{
	//printf("ParseEmptyElemTag: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseChar(itr, '<') && ParseName(itr))
	{
		currentNode->tagName = lastName;

		// (S Attribute)*
		string::iterator itrSave2 = itr;
		while(ParseS(itr) && ParseAttribute(itr))
			itrSave2 = itr;
		itr = itrSave2;
		
		// S?
		ParseS(itr);

		if(ParseString(itr, "/>"))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief EncName ::= [A-Za-z] ([A-Za-z0-9._] | '-')*
 *
 * \sa ParseEncodingDecl()
 */
bool ProtoXmlTree::ParseEncName(string::iterator &itr)
{
	if((*itr >= 'A' && *itr <= 'Z') || (*itr >= 'a' && *itr <= 'z'))
	{
		do
		{
			itr++;
		} while((*itr >= 'A' && *itr <= 'Z') || (*itr >= 'a' && *itr <= 'z') || (*itr >= '0' && *itr <= '9')
				|| *itr == '.' || *itr == '_' || *itr == '-');
		return true;
	}
	return false;
}

/*! \brief EncodingDecl ::= S 'encoding' Eq ('"' EncName '"' | "'" EncName "'" )
 * 
 * \sa ParseXMLDecl()
 */
bool ProtoXmlTree::ParseEncodingDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseS(itr) && ParseString(itr, "encoding") && ParseEq(itr))
	{
		string::iterator itrSave2 = itr;
		if(ParseChar(itr, '"') && ParseEncName(itr) && ParseChar(itr, '"'))
			return true;
		itr = itrSave2;
		if(ParseChar(itr, '\'') && ParseEncName(itr) && ParseChar(itr, '\''))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief EntityDecl ::= GEDecl | PEDecl
 * 
 * \sa ParseMarkupdecl()
 */
bool ProtoXmlTree::ParseEntityDecl(string::iterator &itr)
{
	return ParseGEDecl(itr) || ParsePEDecl(itr);
}

/*! \brief EntityDef ::= EntityValue | (ExternalID NDataDecl?)
 * 
 * \sa ParseGEDecl()
 */
bool ProtoXmlTree::ParseEntityDef(string::iterator &itr)
{
	if(ParseEntityValue(itr))
		return true;
	if(ParseExternalID(itr))
	{
		ParseNDataDecl(itr);
		return true;
	}
	return false;
}

/*! \brief EntityRef ::= '&' Name ';'
 * 
 * This will have to be modified to implement reference inclusion.
 *
 * \sa ParseReference()
 */
bool ProtoXmlTree::ParseEntityRef(string::iterator &itr)
{
	//printf("ParseEntityRef: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseChar(itr, '&') && ParseName(itr) && ParseChar(itr, ';'))
		return true;
	itr = itrSave;
	return false;
}

/*! \brief EntityValue ::=	'"' ([^%&"] | PEReference | Reference)* '"' | "'" ([^%&'] | PEReference | Reference)* "'"
 *
 * \sa ParseEntityDef(), ParsePEDef()
 */
bool ProtoXmlTree::ParseEntityValue(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '"'))
	{
		while(true)
		{
			if(ParsePEReference(itr) || ParseReference(itr)) {}
			else if(*itr != '%' && *itr != '&' && *itr != '"')
				itr++;
			else
				break;
		}
		if(ParseChar(itr, '"'))
			return true;
	}
	itr = itrSave;
	if(ParseChar(itr, '\''))
	{
		while(true)
		{
			if(ParsePEReference(itr) || ParseReference(itr)) {}
			else if(*itr != '%' && *itr != '&' && *itr != '\'')
				itr++;
			else
				break;
		}
		if(ParseChar(itr, '\''))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief EnumeratedType ::= NotationType | Enumeration
 *
 * \sa ParseAttType()
 */
bool ProtoXmlTree::ParseEnumeratedType(string::iterator &itr)
{
	return ParseNotationType(itr) || ParseEnumeration(itr);
}

/*! \brief Enumeration ::= '(' S? Nmtoken (S? '|' S? Nmtoken)* S? ')'
 *
 * \sa ParseEnumeratedType()
 */
bool ProtoXmlTree::ParseEnumeration(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '('))
	{
		ParseS(itr);
		if(ParseNmtoken(itr))
		{
			// (S? '|' S? Nmtoken)* 
			while(true)
			{
				string::iterator itrSave2 = itr;
				ParseS(itr);
				if(!ParseChar(itr, '|'))
				{
					itr = itrSave2;
					break;
				}
				ParseS(itr);
				if(!ParseNmtoken(itr))
				{
					itr = itrSave2;
					break;
				}
			}
			ParseS(itr);
			if(ParseChar(itr, ')'))
				return true;
		}
	}
	itr = itrSave;
	return false;
}

/*! \brief Eq ::= S? '=' S?
 *
 * \sa ParseAttribute(), ParseEncodingDecl(), ParseSDDecl(), ParseVersionInfo()
 */
bool ProtoXmlTree::ParseEq(string::iterator &itr)
{
	//printf("ParseEq: %c\n", *itr);

	string::iterator itrSave = itr;
	ParseS(itr);
	if(ParseChar(itr, '='))
	{
		ParseS(itr);
		return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief ETag ::= '</' Name S? '>'
 *
 * \sa ParseElement()
 */
bool ProtoXmlTree::ParseETag(string::iterator &itr)
{
	//printf("ParseETag: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseString(itr, "</") && ParseName(itr))
	{
		// mismatched end tag
		if(lastName != tagNames.back())
		{
			lastError = "ERROR: end tag name \"" + lastName + "\" does not match start tag name \"" + tagNames.back() + "\"\n";
			return false;
		}
		tagNames.pop_back();

		ParseS(itr);
		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief Extender ::=	(table)
 *
 * See XML 1.0 specification for the full list.
 *
 * \sa ParseNameChar()
 */
bool ProtoXmlTree::ParseExtender(string::iterator &itr)
{
	//printf("ParseExtender: %c\n", *itr);

	static const wchar_t ExtenderTable[11][2] = {
		L'\x00B7', L'\x00B7', L'\x02D0', L'\x02D0', L'\x02D1', L'\x02D1', L'\x0387', L'\x0387', L'\x0640', L'\x0640',
		L'\x0E46', L'\x0E46', L'\x0EC6', L'\x0EC6', L'\x3005', L'\x3005', L'\x3031', L'\x3035', L'\x309D', L'\x309E',
		L'\x30FC', L'\x30FE'
	};

	// use binary search here since it's already written
	int left = 0, right = 14;
	while(left <= right)
	{
		int middle = (int)floor((left + right) / 2.0);
		if(*itr >= ExtenderTable[middle][0] && *itr <= ExtenderTable[middle][1])
		{
			lastChar = *(itr++);
			return true;
		}
		if(*itr < ExtenderTable[middle][0])
			right = middle - 1;
		else
			left = middle + 1;
	}
	return false;
}

/*! \brief ExternalID ::= 'SYSTEM' S SystemLiteral | 'PUBLIC' S PubidLiteral S SystemLiteral
 *
 * \sa ParseDoctypedecl(), ParseEntityDef(), ParseNotationDecl(), ParsePEDef()
 */
bool ProtoXmlTree::ParseExternalID(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "SYSTEM") && ParseS(itr) && ParseSystemLiteral(itr))
		return true;
	itr = itrSave;
	if(ParseString(itr, "PUBLIC") && ParseS(itr) && ParsePubidLiteral(itr) && ParseS(itr) && ParseSystemLiteral(itr))
		return true;
	itr = itrSave;
	return false;
}

/*! \brief GEDecl ::= '<!ENTITY' S Name S EntityDef S? '>'
 *
 * \sa ParseEntityDecl()
 */
bool ProtoXmlTree::ParseGEDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "<!ENTITY") && ParseS(itr) && ParseName(itr) && ParseS(itr) && ParseEntityDef(itr))
	{
		ParseS(itr);
		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief Ideographic ::= (table)
 *
 * See XML 1.0 specification for the full list.
 *
 * \sa ParseLetter()
 */
bool ProtoXmlTree::ParseIdeographic(string::iterator &itr)
{
	//printf("ParseIdeographic: %c\n", *itr);

    // (TBD) further investigate this Ideographic stuff
	/*if(((*itr >= L'\x4E00') && (*itr <= L'\x9FA5')) || 
       (*itr == L'\x3007') || 
       ((*itr >= L'\x3021') && (*itr <= L'\x3029')))
	{
		lastChar = *(itr++);
		return true;
	} */
	return false;
}

/*! \brief intSubset ::= (markupdecl | DeclSep)*
 *
 * \sa ParseDoctypedecl()
 */
bool ProtoXmlTree::ParseIntSubset(string::iterator &itr)
{
	while(ParseMarkupdecl(itr) || ParseDeclSep(itr)) {}
	return true;
}

/*! \brief Letter ::= BaseChar | Ideographic
 *
 * \sa ParseName(), ParseNameChar()
 */ 
bool ProtoXmlTree::ParseLetter(string::iterator &itr)
{
	//printf("ParseLetter: %c\n", *itr);

	return ParseBaseChar(itr) || ParseIdeographic(itr);
}

/*! \brief markupdecl ::= elementdecl | AttlistDecl | EntityDecl | NotationDecl | PI | Comment
 *
 * \sa ParseIntSubset()
 */
bool ProtoXmlTree::ParseMarkupdecl(string::iterator &itr)
{
	return ParseElementdecl(itr) || ParseAttlistDecl(itr) || ParseEntityDecl(itr) || ParseNotationDecl(itr) || ParsePI(itr) || ParseComment(itr);
}

/*! \brief Misc ::= Comment | PI | S
 *
 * \sa ParseDocument(), ParseProlog()
 */
bool ProtoXmlTree::ParseMisc(string::iterator &itr)
{
	return ParseComment(itr) || ParsePI(itr) || ParseS(itr);
}

/*! \brief Mixed ::= \verbatim'(' S? '#PCDATA' (S? '|' S? Name)* S? ')*' | '(' S? '#PCDATA' S? ')'\endverbatim
 *
 * \sa ParseContentspec()
 */
bool ProtoXmlTree::ParseMixed(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '('))
	{
		ParseS(itr);
		if(ParseString(itr, "#PCDATA"))
		{
			// (S? '|' S? Name)*
			while(true)
			{
				string::iterator itrSave2 = itr;
				ParseS(itr);
				if(!ParseChar(itr, '|'))
				{
					itr = itrSave2;
					break;
				}
				ParseS(itr);
				if(!ParseName(itr))
				{
					itr = itrSave2;
					break;
				}
			}
			ParseS(itr);
			if(ParseString(itr, ")*"))
				return true;
		}
	}
	itr = itrSave;
	if(ParseChar(itr, '('))
	{
		ParseS(itr);
		if(ParseString(itr, "#PCDATA"))
		{
			ParseS(itr);
			if(ParseChar(itr, ')'))
				return true;
		}
	}
	itr = itrSave;
	return false;
}

/*! \brief Name ::= (Letter | '_' | ':') (NameChar)*
 *
 * \sa ParseAttDef(), ParseAttlistDecl(), ParseAttribute(), ParseCp(), ParseDoctypedecl(), ParseElementdecl(), ParseEmptyElemTag(), ParseEntityRef(), ParseETag(), ParseGEDecl(), ParseMixed(), ParseNDataDecl(), ParseNotationDecl(), ParseNotationType(), ParsePEDecl(), ParsePEReference(), ParsePITarget(), ParseSTag()
 */
bool ProtoXmlTree::ParseName(string::iterator &itr)
{
	//printf("ParseName: %c\n", *itr);

	if(ParseLetter(itr) || ParseChar(itr, '_') || ParseChar(itr, ':'))
	{
		lastName = "";
		lastName += lastChar;
		while(ParseNameChar(itr))
			lastName += lastChar;
		return true;
	}
	return false;
}

/*! \brief NameChar ::= Letter | Digit | '.' | '-' | '_' | ':' | CombiningChar | Extender
 *
 * \sa ParseName(), ParseNmtoken()
 */
bool ProtoXmlTree::ParseNameChar(string::iterator &itr)
{
	//printf("ParseNameChar: %c\n", *itr);

	return	ParseLetter(itr) || ParseDigit(itr) || ParseChar(itr, '.') || ParseChar(itr, '-') ||
			ParseChar(itr, ':') || ParseCombiningChar(itr) || ParseExtender(itr);
}

/*! \brief NDataDecl ::= S 'NDATA' S Name
 *
 * \sa ParseEntityDef()
 */
bool ProtoXmlTree::ParseNDataDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseS(itr) && ParseString(itr, "NDATA") && ParseS(itr) && ParseName(itr))
		return true;
	itr = itrSave;
	return false;
}

/*! \brief Nmtoken ::= (NameChar)+
 *
 * \sa ParseEnumeration()
 */
bool ProtoXmlTree::ParseNmtoken(string::iterator &itr)
{
	if(ParseNameChar(itr))
	{
		while(ParseNameChar(itr)) {}
		return true;
	}
	return false;
}

/*! \brief NotationDecl ::= '<!NOTATION' S Name S (ExternalID | PublicID) S? '>'
 *
 * \sa ParseMarkupdecl()
 */
bool ProtoXmlTree::ParseNotationDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "<!NOTATION") && ParseS(itr) && ParseName(itr) && ParseS(itr) && (ParseExternalID(itr) || ParsePublicID(itr)))
	{
		ParseS(itr);
		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief NotationType ::= 'NOTATION' S '(' S? Name (S? '|' S? Name)* S? ')'
 *
 * \sa ParseEnumeratedType()
 */
bool ProtoXmlTree::ParseNotationType(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "NOTATION") && ParseS(itr) && ParseChar(itr, '('))
	{
		ParseS(itr);
		if(ParseName(itr))
		{
			// (S? '|' S? Name)*
			while(true)
			{
				string::iterator itrSave2 = itr;
				ParseS(itr);
				if(!ParseChar(itr, '|'))
				{
					itr = itrSave2;
					break;
				}
				ParseS(itr);
				if(!ParseName(itr))
				{
					itr = itrSave2;
					break;
				}
			}
			ParseS(itr);
			if(ParseChar(itr, ')'))
				return true;
		}
	}
	itr = itrSave;
	return false;
}

/*! \brief PEDecl ::= '<!ENTITY' S '%' S Name S PEDef S? '>'
 *
 * \sa ParseEntityDecl()
 */
bool ProtoXmlTree::ParsePEDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "<!ENTITY") && ParseS(itr) && ParseChar(itr, '%') && ParseS(itr) && ParseName(itr) && ParseS(itr) && ParsePEDef(itr))
	{
		ParseS(itr);
		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief PEDef ::= EntityValue | ExternalID
 *
 * \sa ParsePEDecl()
 */
bool ProtoXmlTree::ParsePEDef(string::iterator &itr)
{
	return ParseEntityValue(itr) || ParseExternalID(itr);
}

/*! \brief PEReference ::= '%' Name ';'
 *
 * \sa ParseDeclSep(), ParseEntityValue()
 */
bool ProtoXmlTree::ParsePEReference(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '%') && ParseName(itr) && ParseChar(itr, ';'))
		return true;
	itr = itrSave;
	return false;
}

/*! \brief PI ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
 *
 * \sa ParseContent(), ParseMarkupdecl(), ParseMisc()
 */
bool ProtoXmlTree::ParsePI(string::iterator &itr)
{
	//printf("ParsePI: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseString(itr, "<?") && ParsePITarget(itr))
	{
		// (S (Char* - (Char* '?>' Char*)))?
		ParseS(itr);
		while(!(*itr == '?' && *(itr + 1) == '>'))
			ParseChar(itr);
		if(ParseString(itr, "?>"))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief PITarget ::= Name - (('X' | 'x') ('M' | 'm') ('L' | 'l'))
 *
 * \sa ParsePI()
 */
bool ProtoXmlTree::ParsePITarget(string::iterator &itr)
{
	//printf("ParsePITarget: %c\n", *itr);

	string::iterator itrSave = itr;
	if((ParseChar(itr, 'X') || ParseChar(itr, 'x'))
	&& (ParseChar(itr, 'M') || ParseChar(itr, 'm'))
	&& (ParseChar(itr, 'L') || ParseChar(itr, 'l')))
	{
		itr = itrSave;
		return false;
	}
	itr = itrSave;
	return ParseName(itr);
}

/*! \brief prolog ::= XMLDecl? Misc* (doctypedecl Misc*)?
 *
 * \sa ParseDocument()
 */
bool ProtoXmlTree::ParseProlog(string::iterator &itr)
{
	ParseXMLDecl(itr);
	while(ParseMisc(itr)) {}
	if(ParseDoctypedecl(itr))
	{
		while(ParseMisc(itr)) {}
	}
	return true;
}

/*! \brief PubidChar ::= (table)
 * See XML 1.0 specification for the full list.
 *
 * \sa ParsePubidLiteral()
 */
bool ProtoXmlTree::ParsePubidChar(string::iterator &itr)
{
	if(*itr == '\x20' || *itr == '\xD' || *itr == '\xA' || (*itr >= 'a' && *itr <= 'z') || (*itr >= 'A' && *itr <= 'Z')
	|| (*itr >= '0' && *itr <= '9') || *itr == '-' || *itr == '\'' || *itr == '(' || *itr == ')' || *itr == '+'
	|| *itr == ',' || *itr == '.' || *itr == '/' || *itr == ':' || *itr == '=' || *itr == '?' || *itr == ';'
	|| *itr == '!' || *itr == '*' || *itr == '#' || *itr == '@' || *itr == '$' || *itr == '_' || *itr == '%')
	{
		itr++;
		return true;
	}
	return false;
}

/*! \brief PubidLiteral ::= '"' PubidChar* '"' | "'" (PubidChar - "'")* "'"
 *
 * \sa ParseExternalID(), ParsePublicID()
 */
bool ProtoXmlTree::ParsePubidLiteral(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '"'))
	{
		while(ParsePubidChar(itr)) {}
		if(ParseChar(itr, '"'))
			return true;
	}
	itr = itrSave;
	if(ParseChar(itr, '\''))
	{
		while(*itr != '\'' && ParsePubidChar(itr)) {}
		if(ParseChar(itr, '\''))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief PublicID ::= 'PUBLIC' S PubidLiteral
 *
 * \sa ParseNotationDecl()
 */
bool ProtoXmlTree::ParsePublicID(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "PUBLIC") && ParseS(itr) && ParsePubidLiteral(itr))
		return true;
	itr = itrSave;
	return false;
}

/*! \brief Reference ::= EntityRef | CharRef
 *
 * This will have to be modified to implement reference inclusion.
 *
 * \sa ParseAttValue(), ParseContent(), ParseEntityValue()
 */
bool ProtoXmlTree::ParseReference(string::iterator &itr)
{
	//printf("ParseReference: %c\n", *itr);

	lastReference = "";
	if(ParseEntityRef(itr) || ParseCharRef(itr))
		return true;
	lastReference = "";
	return false;
}

/*! \brief S ::= \verbatim(#x20 | #x9 | #xD | #xA)+\endverbatim
 *
 * Parses whitespace.
 *
 * \sa ParseAttDef(), ParseAttlistDecl(), ParseChoice(), ParseDeclSep(), ParseDefaultDecl(), ParseDoctypedecl(), ParseElementdecl(), ParseEmptyElemTag(), ParseEncodingDecl(), ParseEnumeration(), ParseEq(), ParseETag(), ParseExternalID(), ParseGEDecl(), ParseMisc(), ParseMixed(), ParseNDataDecl(), ParseNotationDecl(), ParseNotationType(), ParsePEDecl(), ParsePI(), ParsePublicID(), ParseSDDecl(), ParseSeq(), ParseSTag(), ParseVersionInfo(), ParseXMLDecl()
 */
bool ProtoXmlTree::ParseS(string::iterator &itr)
{
	//printf("ParseS: %c\n", *itr);

	if(ParseChar(itr, '\x20') || ParseChar(itr, '\x9') || ParseChar(itr, '\xD') || ParseChar(itr, '\xA'))
	{
		while(ParseChar(itr, '\x20') || ParseChar(itr, '\x9') || ParseChar(itr, '\xD') || ParseChar(itr, '\xA')) {}
		return true;
	}
	return false;
}

/*! \brief SDDecl ::= S 'standalone' Eq (("'" ('yes' | 'no') "'") | ('"' ('yes' | 'no') '"'))
 *
 * \sa ParseXMLDecl()
 */
bool ProtoXmlTree::ParseSDDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseS(itr) && ParseString(itr, "standalone") && ParseEq(itr))
	{
		string::iterator itrSave2 = itr;
		if(ParseChar(itr, '\'') && (ParseString(itr, "yes") || ParseString(itr, "no")) && ParseChar(itr, '\''))
			return true;
		itr = itrSave2;
		if(ParseChar(itr, '"') && (ParseString(itr, "yes") || ParseString(itr, "no")) && ParseChar(itr, '\''))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief seq ::= '(' S? cp ( S? ',' S? cp )* S? ')'
 *
 * \sa ParseChildren(), ParseCp()
 */
bool ProtoXmlTree::ParseSeq(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '('))
	{
		ParseS(itr);
		if(ParseCp(itr))
		{
			// ( S? ',' S? cp )*
			while(true)
			{
				string::iterator itrSave2 = itr;
				ParseS(itr);
				if(!ParseChar(itr, ','))
				{
					itr = itrSave2;
					break;
				}
				ParseS(itr);
				if(!ParseCp(itr))
				{
					itr = itrSave2;
					break;
				}
			}
			ParseS(itr);
			if(ParseChar(itr, ')'))
				return true;
		}
	}
	itr = itrSave;
	return false;
}

/*! \brief STag ::= '<' Name (S Attribute)* S? '>'
 *
 * \sa ParseElement()
 */
bool ProtoXmlTree::ParseSTag(string::iterator &itr)
{
	//printf("ParseSTag: %c\n", *itr);

	string::iterator itrSave = itr;
	if(ParseChar(itr, '<') && ParseName(itr))
	{
		currentNode->tagName = lastName;
		tagNames.push_back(lastName);

		// (S Attribute)*
		string::iterator itrSave2 = itr;
		while(ParseS(itr) && ParseAttribute(itr))
			itrSave2 = itr;
		itr = itrSave2;
		
		// S?
		ParseS(itr);

		if(ParseChar(itr, '>'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief Utility function for parsing a string.
 *
 * If the entire string matches, the input is consumed and true is returned;
 * otherwise, nothing is consumed and false is returned.
 *
 * \param s String to be tested against input
 */
bool ProtoXmlTree::ParseString(string::iterator &itr, string s)
{
	string::iterator itrSave = itr;
	for(unsigned int i = 0; i < s.length(); i++)
	{
		if(*(itr++) != s[i])
		{
			itr = itrSave;
			return false;
		}
	}
	return true;
}

/*! \brief StringType ::= 'CDATA'
 *
 * \sa ParseAttType()
 */
bool ProtoXmlTree::ParseStringType(string::iterator &itr)
{
	return ParseString(itr, "CDATA");
}

/*! \brief SystemLiteral ::= ('"' [^"]* '"') | ("'" [^']* "'")
 *
 * \sa ParseExternalID()
 */
bool ProtoXmlTree::ParseSystemLiteral(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseChar(itr, '"'))
	{
		while(*itr != '"' && *itr != '\0')
			itr++;
		if(ParseChar(itr, '"'))
			return true;
	}
	itr = itrSave;
	if(ParseChar(itr, '\''))
	{
		while(*itr != '\'' && *itr != '\0')
			itr++;
		if(ParseChar(itr, '\''))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief TokenizedType ::= 'ID' | 'IDREF' | 'IDREFS' | 'ENTITY' | 'ENTITIES' | 'NMTOKEN' | 'NMTOKENS'
 *
 * \sa ParseAttType()
 */
bool ProtoXmlTree::ParseTokenizedType(string::iterator &itr)
{
	return ParseString(itr, "IDREFS") || ParseString(itr, "IDREF") || ParseString(itr, "ID") || ParseString(itr, "ENTITY")
		|| ParseString(itr, "ENTITIES") || ParseString(itr, "NMTOKENS") || ParseString(itr, "NMTOKEN");
}

/*! \brief VersionInfo ::= S 'version' Eq ("'" VersionNum "'" | '"' VersionNum '"')
 *
 * \sa ParseXMLDecl()
 */
bool ProtoXmlTree::ParseVersionInfo(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseS(itr) && ParseString(itr, "version") && ParseEq(itr))
	{
		string::iterator itrSave2 = itr;
		if(ParseChar(itr, '\'') && ParseVersionNum(itr) && ParseChar(itr, '\''))
			return true;
		itr = itrSave2;
		if(ParseChar(itr, '"') && ParseVersionNum(itr) && ParseChar(itr, '"'))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! \brief VersionNum ::= '1.0'
 *
 * \sa ParseVersionInfo()
 */
bool ProtoXmlTree::ParseVersionNum(string::iterator &itr)
{
	return ParseString(itr, "1.0");
}

/*! \brief XMLDecl ::= '<?xml' VersionInfo EncodingDecl? SDDecl? S? '?>'
 *
 * \sa ParseProlog()
 */
bool ProtoXmlTree::ParseXMLDecl(string::iterator &itr)
{
	string::iterator itrSave = itr;
	if(ParseString(itr, "<?xml") && ParseVersionInfo(itr))
	{
		ParseEncodingDecl(itr);
		ParseSDDecl(itr);
		ParseS(itr);
		if(ParseString(itr, "?>"))
			return true;
	}
	itr = itrSave;
	return false;
}

/*! @} */
