// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: version.c,v 1.2 1998/05/03 22:59:31 killough Exp $
//
//-----------------------------------------------------------------------------

#include "version.h"

// [FG] allow to build reproducibly
#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif
const char version_date[] = BUILD_DATE;
#undef BUILD_DATE

//----------------------------------------------------------------------------
//
// $Log: version.c,v $
// Revision 1.2  1998/05/03  22:59:31  killough
// beautification
//
// Revision 1.1  1998/02/02  13:21:58  killough
// version information files
//
//----------------------------------------------------------------------------
