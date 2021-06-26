#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Interfaces\OSFile.h"
#include "..\Memory\PrivateHeap.h"

namespace RTME
{
	/// <summary>
	/// For sake of performance and thread-safety
	/// </summary>
	class IImportSession
	{
	protected:
		RTMM::PrivateHeap* mHeap;
	public:
		IImportSession(RTMM::PrivateHeap* heap);
		virtual Int32 ReadInto(UInt8* memory, Int32 size) = 0;
		virtual void Relocate(Int32 offset, RTI::LocateOption option) = 0;
		virtual ~IImportSession();

		template<class T>
		bool ReadInto(T& target) {
			Int32 readBytes = ReadInto((UInt8*)&target, sizeof(T));
			return readBytes == sizeof(T);
		}

		template<class T>
		bool ReadIntoSeries(Int32& countTarget, T*& target) {
			Int32 readBytes = ReadInto((UInt8*)&countTarget, sizeof(Int32));
			if (readBytes != sizeof(Int32))
				return false;
			target = new (mHeap) T[countTarget];
			readBytes = ReadInto((UInt8*)target, countTarget * sizeof(T));
			return readBytes == countTarget * sizeof(T);
		}

		template<class T>
		bool ReadIntoSeriesDirectly(Int32& countTarget, T*& target) {
			Int32 readBytes = ReadInto((UInt8*)&countTarget, sizeof(Int32));
			if (readBytes != sizeof(Int32))
				return false;
			readBytes = ReadInto((UInt8*)target, countTarget * sizeof(T));
			return readBytes == countTarget * sizeof(T);
		}
	};
}