#pragma once

#include "FcWebApi.h"

#include <string>


namespace FusionCrowdWeb
{
	class FC_WEB_API FcFileWrapper
	{
	public:
		FcFileWrapper();
		explicit FcFileWrapper(const char* inFileName);

		FcFileWrapper(FcFileWrapper &&);
		FcFileWrapper & operator=(FcFileWrapper &&);

		~FcFileWrapper();

		size_t GetSize() const;
		void SetFileName(const char* inFileName);
		void SetFileNameAsResource(const char* inFileName);
		const char* GetFileName();

		void WriteToMemory(char*& outMemoryIterator) const;
		void ReadFromMemory(const char*& outMemoryIterator);

		void Unwrap();

		static size_t GetFileSize(const char* inFileName);
		static std::string GetFullNameForResource(const char* inFileName);

	private:
		char* _fileName = nullptr;
		char* _fileData = nullptr;
		size_t _fileNameSize = 0;
		size_t _fileDataSize = 0;
	};
}
