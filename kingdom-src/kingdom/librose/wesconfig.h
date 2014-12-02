
#ifndef WESCONFIG_H_INCLUDED
#define WESCONFIG_H_INCLUDED

/**
 * @file wesconfig.h
 * Some defines: PACKAGE
 *
 * This file should only be modified by the packager of the tarball
 * before and after each release.
 */

#define ROSE_VERSION	"0.0.1"

//always use the version string in here, otherwise autotools can override in
//a bad way...
#ifndef PACKAGE
#define PACKAGE "wesnoth"
#endif

#endif
