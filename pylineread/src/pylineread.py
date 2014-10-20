#!/usr/bin/env python

import numpy as np
import scipy.constants as sc
import sys, os, time

import ConfigParser
import argparse
import struct
import heapq as hq

#import hitran    as hit
import constants as c
import utils     as ut
import db_pands  as ps
import db_hitran as hit
import db_tioschwenke as ts
sys.path.append("/home/patricio/ast/esp01/code/bart/fortran/TIPS_2011")


def parseargs():
  """
  Read and process the command line arguments.

  Returns:
  --------
  args: Namespace object
     Object with the command line arguments.

  Modification History:
  ---------------------
  2013        madison   Initial implementation.        
  2014-03-06  patricio  Updated from optparse to argparse. Added documentation
                        and config_file option.    pcubillos@fulbrightmail.org
  """
  # Parser to process a configuration file:
  cparser = argparse.ArgumentParser(description=__doc__, add_help=False,
                           formatter_class=argparse.RawDescriptionHelpFormatter)
  # Add config file option:
  cparser.add_argument("-c", "--config_file",
                       help="Specify config file", metavar="FILE")
  # remaining_argv contains all other command-line-arguments:
  args, remaining_argv = cparser.parse_known_args()

  # Get parameters from configuration file (if exists):
  if args.config_file:
    if not os.path.isfile(args.config_file):
      ut.printexit("Configuration file '%s' does not exist."%args.config_file)
    config = ConfigParser.SafeConfigParser()
    config.read([args.config_file])
    if "Parameters" not in config.sections():
      ut.printexit("Invalid configuration file: '%s'."%args.config_file)
    defaults = dict(config.items("Parameters"))
    # Store these arguments as lists:
    if "db_list" in defaults:
      defaults["db_list"]   = defaults["db_list"].split()
    if "part_list" in defaults:
      defaults["part_list"] = defaults["part_list"].split()
    if "dbtype" in defaults:
      defaults["dbtype"]    = defaults["dbtype"].split()
  else:
    defaults = {}

  # Inherit options from cparser:
  parser = argparse.ArgumentParser(parents=[cparser])

  # General Options:
  parser.add_argument("-v", "--verbose-level", action="store",
                       help="Verbosity level (integer) [default: %(default)s]",
                       dest="verb", type=int, default=2)
  parser.add_argument("-q", "--quiet",         action="store_false",
                       help="Set verbosity level to 0.",
                       dest="verb")
  # Database Options:
  group = parser.add_argument_group("Database Options")
  group.add_argument("-o", "--output",
                     action  = "store",
                     help    = "Output filename [default: '%(default)s']",
                     dest    = "output",
                     default = "output.tli")
  group.add_argument("-d", "--database",       action="append",
                     help="Database filename",
                     dest="db_list")
  group.add_argument("-p", "--partition",      action="append",
                     help="Auxiliary partition function filename",
                     dest="part_list")
  group.add_argument("-t", "--dbtype",         action="append",
                     help="Database type, select: [pands, hit]",
                     dest="dbtype")
  # Wavelength Options:
  group = parser.add_argument_group("Wavelength Options")
  group.add_argument("-i", "--wav-init",       action="store",
                     help="Initial wavelength (microns) [default: %(default)s]",
                     dest="iwav", type=float, default=1.0)
  group.add_argument("-f", "--wav-final",      action="store",
                     help="Final wavelength (microns) [default: %(default)s]",
                     dest="fwav", type=float, default=1.1)
  parser.set_defaults(**defaults)
  args = parser.parse_args(remaining_argv)

  return args


if __name__ == "__main__":
  """
  Prototype code for porting lineread to python with P&S DB support
  Multiple DB types forthcoming with rolling revisions

  Parameters:
  -----------

  Modification History:
  ---------------------
  2013-10-21  madison   Initial version based on P. Rojo's lineread C code.
                                                 madison.stemm@ucf.edu
  2014-03-05  patricio  Added documentation and updated Madison's code.
                                                 pcubillos@fulbrightmail.org
  2014-07-27  patricio  Updated to version 5.0
  """

  # Process command-line-arguments:
  cla = parseargs()
  # Unpack parameters:
  verbose    = cla.verb
  dblist     = cla.db_list
  pflist     = cla.part_list
  dbtype     = cla.dbtype 
  outputfile = cla.output

  # Number of files:
  Nfiles = len(dblist)
  # Driver routine to read the databases:
  driver = []
  for i in np.arange(Nfiles):
    if   dbtype[i] == "ps":
      driver.append(ps.pands(dblist[i], pflist[i]))
    elif dbtype[i] == "hit":
      driver.append(hit.hitran(dblist[i], pflist[i]))
    elif dbtype[i] == "ts":
      driver.append(ts.tioschwenke(dblist[i], pflist[i]))
    else:
      ut.printexit("Unknown Database type (%d): '%s'"%(i+1, dbtype[i]))
    ut.lrprint(verbose-10, "File %d, database name: '%s'"%(i+1, driver[i].name))

  ut.lrprint(verbose, "Beginning to write the TLI file: '%s'"%outputfile)
  # Open output file:
  TLIout  = open(outputfile, "wb")

  # Get the machine endian type (big/little):
  endianness = sys.byteorder

  # Hardcoded implementation of lineread's "magic number" check for endianness
  # derived from binary encoding the letters TLI into binary location
  # and checking the order
  if endianness == 'big':
    magic = '\xab\xb3\xb6\xff'
  if endianness == 'little':
    magic = '\xff\xb6\xb3\xab'

  # TLI header: Tells endianness of binary, TLI version, and number of
  # databases used.
  header = magic
  header += struct.pack("3h", c.TLI_VERSION, c.LR_VERSION, c.LR_REVISION)
  # Add initial and final wavelength boundaries:
  header += struct.pack("2d", cla.iwav, cla.fwav)

  # Get number of databases:
  DBnames = [] # Database names
  DBskip  = [] # Index of repeated databases
  for i in np.arange(Nfiles):
    dbname = driver[i].name
    if dbname in DBnames:
      DBskip.append(i) # Ommit repeated databases
    else:
      DBnames.append(dbname) 
  Ndb = len(DBnames)
  header += struct.pack("h", Ndb)
  TLIout.write(header)

  ut.lrprint(verbose-8, "Magic number: %d\n"
                        "TLI Version:  %d\n"
                        "LR Version:   %d\n"
                        "LR Revision:  %d\n"
                        "Initial wavelength (um): %7.3f\n"
                        "Final   wavelength (um): %7.3f"%(
                         struct.unpack('i', magic)[0],
                         c.TLI_VERSION, c.LR_VERSION, c.LR_REVISION,
                         cla.iwav, cla.fwav))
  ut.lrprint(verbose-8, "There are %d databases in %d files."%(Ndb, Nfiles))
  ut.lrprint(verbose-9, "Databases: " + str(DBnames))

  # Partition info:
  totIso = 0                  # Cumulative number of isotopes
  acum = np.zeros(Ndb+1, int) # Cumul. number of isotopes per database

  ut.lrprint(verbose-2, "Reading and writting partition function info.")
  # Database correlative number:
  idb = 0
  # Loop through the partition files (if more than one) and write the
  # data to a processed TLI file:
  for i in np.arange(Nfiles):
    # Skip if we already stored the PF info of this DB:
    if i in DBskip:
      continue

    # Get partition function values:
    Temp, partDB = driver[i].getpf(verbose)
    isoNames  = driver[i].isotopes
    iso_mass  = driver[i].mass
    iso_ratio = driver[i].isoratio

    ut.lrprint(verbose, "Isotopes mass: " + str(iso_mass))

    # Number of temperature samples:
    Ntemp = len(Temp)
    # Number of isotopes:
    Niso  = len(isoNames)

    # Get file name (remove root and extension):
    lenDBname = len(DBnames[idb])
    lenMolec  = len(driver[i].molecule)

    # Store length of database name, database name, number of temperatures,
    #  and number of isotopes in TLI file:
    TLIout.write(struct.pack("h%dc"%lenDBname,  lenDBname, *DBnames[idb]))
    # Store the molecule name:
    TLIout.write(struct.pack("h%dc"%lenMolec, lenMolec, *driver[i].molecule))
    # Store the number of temperature samples and isotopes:
    TLIout.write(struct.pack("hh", Ntemp, Niso))
    ut.lrprint(verbose-8, "Database (%d/%d): '%s (%s molecule)'\n"
                          "  Number of temperatures: %d\n"
                          "  Number of isotopes: %d"%(idb+1, Ndb, DBnames[idb],
                                              driver[i].molecule, Ntemp, Niso))

    # Write all the temperatures in a list:
    TLIout.write(struct.pack("%dd"%Ntemp, *Temp))
    ut.lrprint(verbose-8, "  Temperatures: [%6.1f, %6.1f, ..., %6.1f]"%(
                             Temp[0], Temp[1], Temp[Ntemp-1]))

    # For each isotope, write the relevant partition function information
    # to file.  Keep a tally of isotopes for multiple databse support 
    for j in np.arange(Niso):
      ut.lrprint(verbose-9, "  Isotope (%d/%d): '%s'"%(j+1, Niso, isoNames[j]))

      # Store length of isotope name, isotope name, and isotope mass:
      lenIsoname = len(isoNames[j])
      Iname = str(isoNames[j])
      TLIout.write(struct.pack("h%dc"%lenIsoname, lenIsoname, *Iname))
      TLIout.write(struct.pack("d", iso_mass[j]))
      TLIout.write(struct.pack("d", iso_ratio[j]))

      # Write the partition function per isotope:
      TLIout.write(struct.pack("%dd"%Ntemp, *partDB[j]))
      ut.lrprint(verbose-9, "    Mass (u):        %8.4f\n"
                            "    Isotopic ratio:  %8.4g\n"
                            "    Part. Function:  [%.2e, %.2e, ..., %.2e]"%(
                             iso_mass[j], iso_ratio[j],
                             partDB[j,0], partDB[j,1], partDB[j,Ntemp-1]))

    # Calculate cumulative number of isotopes per database:
    totIso += Niso
    idb += 1
    acum[idb] = totIso

  # Cumulative number of isotopes:
  ut.lrprint(verbose-5, "Cumulative number of isotopes per DB: " + str(acum))
  ut.lrprint(verbose, "Done.")

  ut.lrprint(verbose, "\nWriting transition info to tli file:")
  wlength = []
  gf      = []
  elow    = []
  isoID   = []
  # Read from file and write the transition info:
  for db in np.arange(Nfiles):
    # Get database index:
    dbname = driver[db].name
    idb = DBnames.index(dbname)

    # Read databases:
    ti = time.time()
    transDB = driver[db].dbread(cla.iwav, cla.fwav, cla.verb, pflist[db])
    tf = time.time()
    ut.lrprint(verbose-3, "Reading time: %8.3f seconds"%(tf-ti))
    
    wlength = np.concatenate((wlength, transDB[0]))    
    gf      = np.concatenate((gf,      transDB[1]))    
    elow    = np.concatenate((elow,    transDB[2]))    
    isoID   = np.concatenate((isoID,   transDB[3]+acum[idb]))    

    ut.lrprint(verbose-8, "Isotpe in-database indices: " +
                          str(np.unique(transDB[3])))
    ut.lrprint(verbose-8, "Isotpe correlative indices: " +
                          str(np.unique(transDB[3]+acum[idb])))

  # Sort the line transitions (by wavelength):
  ti = time.time()
  wlsort = np.argsort(wlength)
  tf = time.time()
  ut.lrprint(verbose-3, " Sort  time: %8.7f seconds"%(tf-ti))
  wlength = wlength[wlsort]
  gf      = gf     [wlsort]
  elow    = elow   [wlsort]
  isoID   = isoID  [wlsort]

  nTransitions = np.size(wlength) # Total number of transitions
  ut.lrprint(verbose-3, "Writing %d transition lines."%nTransitions)

  # Pack:
  transinfo = ""
  # Write the number of transitions:
  TLIout.write(struct.pack("i", nTransitions))

  ti = time.time()
  transinfo += struct.pack(str(nTransitions)+"d", *list(wlength))
  transinfo += struct.pack(str(nTransitions)+"h", *list(isoID))
  transinfo += struct.pack(str(nTransitions)+"d", *list(elow))
  transinfo += struct.pack(str(nTransitions)+"d", *list(gf))
  tf = time.time()
  ut.lrprint(verbose-3, "Packing time: %8.3f seconds"%(tf-ti))

  ti = time.time()
  TLIout.write(transinfo)
  tf = time.time()
  ut.lrprint(verbose-3, "Writing time: %8.3f seconds"%(tf-ti))

  TLIout.close()
  ut.lrprint(verbose, "Done.\n")
  sys.exit(0)
