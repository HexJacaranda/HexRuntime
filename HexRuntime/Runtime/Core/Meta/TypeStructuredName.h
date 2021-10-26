#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\..\Range.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace RTM
{
	class TypeStructuredName;

	struct TypeNameNode
	{
		IndexRange FullRange;
		IndexRange Canonical;
		std::vector<std::shared_ptr<TypeStructuredName>> Arguments;
	};

	/// <summary>
	/// Type need structured name for generic resolving
	/// </summary>
	class TypeStructuredName
	{
		friend class TypeNameParser;
		std::wstring mOriginString;
		IndexRange mReferenceAssembly = { 0,0 };
		IndexRange mNamespace = { 0,0 };
		std::vector<TypeNameNode> mTypeNameNodes;
	public:
		TypeStructuredName() = default;
		std::wstring_view GetReferenceAssembly()const;
		std::wstring_view GetNamespace()const;
		std::wstring_view GetTypeName()const;
		std::wstring_view GetShortTypeName()const;
		std::vector<TypeNameNode> const& GetTypeNodes()const;
		std::wstring_view GetFullyQualifiedName()const;
		std::shared_ptr<TypeStructuredName> InstantiateWith(std::vector<std::wstring_view> const& arguments);
	private:
		template<class Fn>
		static IndexRange TrackRange(std::wstring const& toTrack, Fn&& action)
		{
			Int32 begin = toTrack.size();
			std::forward<Fn>(action)();
			Int32 end = toTrack.size();
			return { begin, end - begin };
		}
	};

#define FAIL_PARSE(RET) mParseFailed = true; return RET
#define IF_FAIL_PARSE_RET(RET) if(mParseFailed) return RET
#define PARSE_CALL(EXPR, RET) EXPR;  IF_FAIL_PARSE_RET(RET)

	class TypeNameParser
	{
		bool mParseFailed = false;
		std::wstring mOrigin;
		Int32 mIndex;
		std::shared_ptr<TypeStructuredName> mParsedName;
	public:
		TypeNameParser(std::wstring_view originName);
		std::shared_ptr<TypeStructuredName> Parse(bool suppressArgumentParse = false);
	private:
		IndexRange ParseAssembly();
		IndexRange ParseNamespace();
		wchar_t ParseTypeNameNode(TypeNameNode& outNode);

		wchar_t PeekNextTypeArgument(Int32& index);
		wchar_t Peek();
		void Expect(wchar_t value);
		wchar_t AcceptIdentifier();
	};
}