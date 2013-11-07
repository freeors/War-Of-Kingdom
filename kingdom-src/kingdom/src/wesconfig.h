
#ifndef WESCONFIG_H_INCLUDED
#define WESCONFIG_H_INCLUDED

/**
 * @file wesconfig.h
 * Some defines: VERSION, PACKAGE
 *
 * This file should only be modified by the packager of the tarball
 * before and after each release.
 */
#ifndef LOCALEDIR
#  define LOCALEDIR "translations"
#endif

//always use the version string in here, otherwise autotools can override in
//a bad way...
#ifdef VERSION
  #undef VERSION
#endif
#define VERSION "1.0.17"
#ifndef PACKAGE
#define PACKAGE "wesnoth"
#endif

#endif
