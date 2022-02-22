#include "AssemblyContext.h"

std::pmr::synchronized_pool_resource* RTM::GetDefaultResource()
{
	static std::pmr::synchronized_pool_resource resource;
	return &resource;
}

RTM::AssemblyContext::AssemblyContext(RTMM::PrivateHeap* heap, RTME::MDImporter* importer) :
	Heap(heap),
	Importer(importer),
	Entries(GetDefaultResource())
{

}