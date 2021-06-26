#include "AssemblyContext.h"

RTM::AssemblyContext::AssemblyContext(RTMM::PrivateHeap* heap, RTME::MDImporter* importer):
	Heap(heap),
	Importer(importer)
{

}