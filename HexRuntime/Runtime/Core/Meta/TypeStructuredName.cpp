#include "TypeStructuredName.h"
#include "..\..\Utility.h"

std::wstring_view RTM::TypeStructuredName::GetReferenceAssembly() const
{
	return mReferenceAssembly[mOriginString];
}

std::wstring_view RTM::TypeStructuredName::GetNamespace() const
{
	return mNamespace[mOriginString];
}

std::wstring_view RTM::TypeStructuredName::GetShortTypeName() const
{
	return mTypeNameNodes[mTypeNameNodes.size() - 1].FullRange[mOriginString];
}

std::vector<RTM::TypeNameNode> const& RTM::TypeStructuredName::GetTypeNodes() const
{
	return mTypeNameNodes;
}

std::wstring_view RTM::TypeStructuredName::GetFullyQualifiedName() const
{
	return mOriginString;
}

std::shared_ptr<RTM::TypeStructuredName> RTM::TypeStructuredName::InstantiateWith(std::vector<std::wstring_view> const& arguments)
{
	auto ret = std::make_shared<RTM::TypeStructuredName>();
	auto&& newNodes = ret->mTypeNameNodes;
	auto&& newName = ret->mOriginString;
	
	newName.push_back(L'[');
	ret->mReferenceAssembly = TrackRange(newName, [&]() { newName.append(GetReferenceAssembly()); });
	newName.push_back(L']');
		
	newName.push_back(L'[');
	ret->mNamespace = TrackRange(newName, [&]() { newName.append(GetNamespace()); });
	newName.push_back(L']');

	Int32 inValueIndex = 0;

	//Local function to handle whether we should use parameters
	auto handleArgument = [&](TypeNameNode& newNode, std::shared_ptr<TypeStructuredName> const& oldName) {
		//Assume suppress argument parsing is on
		auto oldTypeName = oldName->GetFullyQualifiedName();
		if (oldTypeName == L"Canon")
		{
			newName.append(arguments[inValueIndex]);
			TypeNameParser parser{ arguments[inValueIndex] };
			newNode.Arguments.push_back(parser.Parse());
			inValueIndex++;
		}
		else
		{
			newName.append(oldTypeName);
			newNode.Arguments.push_back(oldName);
		}
	};

	Enumerable::ContractIterate(mTypeNameNodes,
		[&](auto&& node) 
		{
			TypeNameNode newNode{};
			//Track full name
			newNode.FullRange = TrackRange(newName,
				[&]() {
					//Track canonical name
					newNode.Canonical = TrackRange(newName,
						[&]() {
							newName.append(node.Canonical[mOriginString]);
						});
					if (node.Arguments.size() > 0)
					{
						newName.push_back(L'<');

						Enumerable::ContractIterate(node.Arguments,
							[&](auto&& argument) 
							{
								handleArgument(newNode, argument);
							},
							[&](auto&& _) 
							{
								newName.append(L", ");
							});		

						newName.push_back(L'>');
					}
				});

			ret->mTypeNameNodes.push_back(std::move(newNode));
		},
		[&](auto&& _) 
		{
			newName.push_back(L'.');
		});

	return ret;
}


std::wstring RTM::TypeStructuredName::GetCanonicalizedName() const
{
	std::wstring ret{};

	ret.push_back(L'[');
	ret.append(GetReferenceAssembly());
	ret.push_back(L']');

	ret.push_back(L'[');
	ret.append(GetNamespace());
	ret.push_back(L']');

	Enumerable::ContractIterate(mTypeNameNodes,
		[&](TypeNameNode const& node)
		{
			ret.append(node.Canonical[mOriginString]);
			if (node.Arguments.size() > 0)
			{
				ret.push_back(L'<');

				Enumerable::ContractIterate(node.Arguments,
					[&](auto&& _) {
						ret.append(CanonicalPlaceholder);
					},
					[&](auto&& _) {
						ret.append(L", ");
					});

				ret.push_back(L'>');
			}
		},
		[&](TypeNameNode const& _)
		{
			ret.push_back(L'.');
		});

	return ret;
}

bool RTM::TypeStructuredName::IsFullyCanonical() const
{
	for (auto&& node : mTypeNameNodes)
	{
		if (node.FullRange[mOriginString] == CanonicalPlaceholder)
			return true;

		for (auto&& arg : node.Arguments)
			if (arg->GetFullyQualifiedName() != CanonicalPlaceholder)
				return false;
	}
			
	return true;
}

RTM::TypeNameParser::TypeNameParser(std::wstring_view originName)
	:mOrigin(originName), mIndex(0)
{
	mParsedName = std::make_shared<TypeStructuredName>();
}

std::shared_ptr<RTM::TypeStructuredName> RTM::TypeNameParser::Parse()
{
	//Canonical holder
	if (mOrigin != CanonicalPlaceholder)
	{
		mParsedName->mReferenceAssembly = ParseAssembly();
		IF_FAIL_PARSE_RET(nullptr);
		mParsedName->mNamespace = ParseNamespace();
		IF_FAIL_PARSE_RET(nullptr);
	
		wchar_t lastCh = L'\0';
		while (true)
		{
			TypeNameNode nameNode{};
			lastCh = ParseTypeNameNode(nameNode);
			if (lastCh == L'\0')
			{
				mParsedName->mTypeNameNodes.push_back(std::move(nameNode));
				break;
			}			
			else if (lastCh == L'.')
			{
				mParsedName->mTypeNameNodes.push_back(std::move(nameNode));
				//Advance
				mIndex++;
			}
			else
			{
				FAIL_PARSE(nullptr);
			}
		}
	}

	//Move completed name to parsed name
	mParsedName->mOriginString = std::move(mOrigin);
	return std::move(mParsedName);
}

std::wstring_view RTM::TypeNameParser::ExtractAssembly()
{
	auto&& range = ParseAssembly();
	return std::wstring_view{ mOrigin }.substr(range.Base, range.Count);
}

std::wstring_view RTM::TypeNameParser::ExtractAssembly(std::wstring_view originName)
{
	TypeNameParser parser{ originName };
	auto&& range = parser.ParseAssembly();
	return std::wstring_view{ originName }.substr(range.Base, range.Count);
}


RT::IndexRange RTM::TypeNameParser::ParseAssembly()
{
	PARSE_CALL(Expect(L'['), {});
	Int32 baseIndex = mIndex;
	PARSE_CALL(AcceptIdentifier(), {});
	Int32 endIndex = mIndex;
	PARSE_CALL(Expect(L']'), {});
	return IndexRange{ baseIndex, endIndex - baseIndex };
}

RT::IndexRange RTM::TypeNameParser::ParseNamespace()
{
	PARSE_CALL(Expect(L'['), {});

	Int32 baseIndex = mIndex;
	while (true)
	{
		PARSE_CALL(wchar_t tailSymbol = AcceptIdentifier(), {});

		if (tailSymbol == L']')
		{
			Int32 endIndex = mIndex;
			PARSE_CALL(Expect(L']'), {});
			return IndexRange{ baseIndex, endIndex - baseIndex };
		}
		else if (tailSymbol == L'.')
		{
			mIndex++;
		}
		else
		{
			FAIL_PARSE({});
		}
	}
}

wchar_t RTM::TypeNameParser::ParseTypeNameNode(TypeNameNode& outNode)
{
	Int32 baseIndex = mIndex;
	wchar_t lastCh = AcceptIdentifier();
	Int32 canonicalIndex = mIndex;

	outNode.Canonical = IndexRange{ baseIndex, canonicalIndex - baseIndex };
	mIndex++;
	
	if (lastCh == L'<')
	{
		for (;;)
		{
			Int32 typeArgumentLeftIndex = mIndex;
			Int32 typeArgumentRightIndex = 0;
			//Should scan forward
			lastCh = PeekNextTypeArgument(typeArgumentRightIndex);
			TypeNameParser parser{
				std::wstring_view{mOrigin}.substr(typeArgumentLeftIndex, typeArgumentRightIndex - typeArgumentLeftIndex)
			};
			outNode.Arguments.push_back(parser.Parse());

			if (lastCh == L'>')
			{
				//<A>
				mIndex = typeArgumentRightIndex + 1;
				lastCh = Peek();
				break;
			}
			else if (lastCh == L',')
			{
				//A, B
				mIndex = typeArgumentRightIndex + 2;
			}
			else
			{
				FAIL_PARSE(L'\0');
			}
		}
	}

	outNode.FullRange = IndexRange{ baseIndex, mIndex - baseIndex };
	return lastCh;
}

wchar_t RTM::TypeNameParser::PeekNextTypeArgument(Int32& index)
{
	Int32 leftBrace = 0;
	index = mIndex;
	while (index < mOrigin.size())
	{
		if (mOrigin[index] == L'<')
			leftBrace++;
		else if (mOrigin[index] == L'>')
		{
			if (leftBrace > 0)
				leftBrace--;
			else if (leftBrace == 0)
				return mOrigin[index];
			else
			{
				//Unexpected
				FAIL_PARSE(L'\0');
			}

			if (leftBrace == 0)
				return mOrigin[index];
		}
		else if (mOrigin[index] == L',')
		{
			if (leftBrace == 0)
				return mOrigin[index];
		}
		index++;
	}
	//Unexpected
	FAIL_PARSE(L'\0');
}

wchar_t RTM::TypeNameParser::Peek()
{
	if (mIndex < mOrigin.size())
		return mOrigin[mIndex];
	else
		return L'\0';
}

void RTM::TypeNameParser::Expect(wchar_t value)
{
	if (mOrigin[mIndex++] != value)
		mParseFailed = true;
}

wchar_t RTM::TypeNameParser::AcceptIdentifier()
{
	auto isMatch = [](wchar_t ch) {
		return (L'a' <= ch && ch <= L'z') || (L'A' <= ch && ch <= L'Z') || (L'0' <= ch && ch <= L'9') || (ch == L'_');
	};
	while (mIndex < mOrigin.size() && isMatch(mOrigin[mIndex])) mIndex++;

	return Peek();
}
