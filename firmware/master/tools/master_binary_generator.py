#
# Python script to execute the compilation
# of TC cube firmware utilizing different hard coded
# addresses.
#
# Sifteo 2012 
#
# Jared Wolff - Sifteo Hardware Engineering

import sys, os, platform
import shutil, datetime
import subprocess, multiprocessing

########################################################
## Defines
########################################################
DIR = os.path.dirname(os.path.realpath(__file__))
TC_DIR = os.path.normpath(os.path.join(DIR, "..", "..", ".."))
MASTER_UNVERSIONED = "master-stm32.sft"
LAUNCHER_UNVERSIONED = "launcher.elf"
CLEAN = True

####################################
## Checks to see if directory exists.
## Deletes old one if it does exist
####################################
def check_and_mkdir(path):
    # ensure our destination exists
    try:
        os.mkdir(path)
    except:
        pass

####################################
## Check to make sure we're in the SDK shell
####################################
def ensureRunningFromSDKShell():
    if  os.getenv("SDK_DIR") == None:
        error("SDK directory not defined!")

####################################
## Error dialog
####################################
def error(detail):
    print "########################################################"
    print "## ERROR: ", detail
    print "########################################################"
    sys.exit(1)

########################################################################
## Copies raw filename renamed to the versioned_filename at the target path
########################################################################
def copy_to_dir(raw_filename,versioned_filename,target_path):
    shutil.copy(raw_filename, os.path.join(target_path, versioned_filename))
    print "#### Moving %s to %s" % ( versioned_filename, target_path )

##############################
## Our main execution function
##############################
def run(secondary_path, build_launcher, gen):
    print "\n#### Master Binary Generator\n"
    print "\n#### Generating Gen %d Files\n" % gen

    #grab githash
    githash = subprocess.check_output(["git", "describe", "--tags"]).strip()

    #set up directory pointers.
    latest_dir = os.path.join(DIR, "..", "latest")
    launcher_dir = os.path.join(TC_DIR, "launcher")
    build_dir = os.path.join(DIR, "..", githash)

    #one time operations if secondary_path is defined
    if secondary_path != False:
      remote_build_dir = os.path.join(secondary_path,githash)
      remote_latest_dir = os.path.join(secondary_path,"latest")

    else:
      remote_build_dir = None
      remote_latest_dir = None

    #master firmware filename
    master_filename = "master_%s_gen%d.sft" % (githash,gen)

    #launcher filename
    launcher_filename = "launcher_%s.elf" % (githash)

    # one time operations
    date = datetime.date.today().isoformat()

    ############################
    #### Process base firmware
    ############################

    #run make clean on master firmware directory
    if CLEAN:
      subprocess.check_call(["make", "clean"])

    check_and_mkdir(build_dir)
    check_and_mkdir(latest_dir)

    # Set dem environment variables
    myenv = dict(os.environ)
    myenv["BOOTLOADABLE"] = "1"

    if gen == 3:
        myenv["BOARD"] = "BOARD_TC_MASTER_REV3"
    else:
        myenv["BOARD"] = "BOARD_TC_MASTER_REV2"

    # Compile!
    cores = "-j%d" % multiprocessing.cpu_count()
    subprocess.call(["make", cores, "encrypted"], env=myenv)

    # Do a little renaming
    copy_to_dir(MASTER_UNVERSIONED,master_filename,latest_dir)
    copy_to_dir(MASTER_UNVERSIONED,master_filename,build_dir)

    if secondary_path != False:

        check_and_mkdir(remote_latest_dir)
        check_and_mkdir(remote_build_dir)

        copy_to_dir(MASTER_UNVERSIONED,master_filename, remote_latest_dir)
        copy_to_dir(MASTER_UNVERSIONED,master_filename, remote_build_dir)

     ############################
     #### Process launcher
     ############################

    if build_launcher:

        ensureRunningFromSDKShell()

        ### Changing directories!!!
        os.chdir( launcher_dir )

        print "#### Changing directory to %s." % launcher_dir

        if CLEAN:
          subprocess.check_call(["make", "clean"])

        subprocess.check_call(["make","RELEASE=1"])

        copy_to_dir(LAUNCHER_UNVERSIONED,launcher_filename,latest_dir)
        copy_to_dir(LAUNCHER_UNVERSIONED,launcher_filename,build_dir)

        if secondary_path != False:
            copy_to_dir(LAUNCHER_UNVERSIONED,launcher_filename,remote_latest_dir)
            copy_to_dir(LAUNCHER_UNVERSIONED,launcher_filename,remote_build_dir)

    #Prints out version at the end for any excel copy paste action
    print "#### Git Version: %s" % githash
    
if __name__ == '__main__':

    if len(sys.argv) > 3:
        print >> sys.stderr, "usage: python master_binary_generator.py --secondary-path <secondary path> --gen <gen #> --with-launcher"
        sys.exit(1)

    secondary_path = False
    build_launcher = False
    gen = 2

    for idx,arg in enumerate(sys.argv):
        if arg == "--with-launcher":
            build_launcher = True
        elif arg == "--secondary-path":
            secondary_path = sys.argv[idx+1]
        elif arg == "--gen":
            gen = int(sys.argv[idx+1])

    run(secondary_path, build_launcher, gen)
