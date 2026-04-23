
set(SOURCES
	multiplaymgr.cxx
	tiny_xdr.cxx
	MPServerResolver.cxx
	mpirc.cxx
	cpdlc.cxx
	)

set(HEADERS
	multiplaymgr.hxx
	tiny_xdr.hxx
	MPServerResolver.hxx
	mpirc.hxx
	cpdlc.hxx
	mpmessages.hxx
	)
    	
target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
