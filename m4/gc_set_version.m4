#
# THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
# OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
#
# Permission is hereby granted to use or copy this program
# for any purpose, provided the above notices are retained on all copies.
# Permission to modify the code and to distribute modified code is granted,
# provided the above notices are retained, and a notice that the code was
# modified is included with the above copyright notice.

# MANAGED_STACK_ADDRESS_BOEHM_GC_SET_VERSION
# sets and AC_DEFINEs MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR, MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR and MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO
# based on the contents of PACKAGE_VERSION; PACKAGE_VERSION must conform to
# [0-9]+[.][0-9]+[.][0-9]+
#
AC_DEFUN([MANAGED_STACK_ADDRESS_BOEHM_GC_SET_VERSION], [
  AC_MSG_CHECKING(GC version numbers)
  MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR=`echo $PACKAGE_VERSION | sed 's/^\([[0-9]][[0-9]]*\)[[.]].*$/\1/g'`
  MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR=`echo $PACKAGE_VERSION | sed 's/^[[^.]]*[[.]]\([[0-9]][[0-9]]*\).*$/\1/g'`
  MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO=`echo $PACKAGE_VERSION | sed 's/^[[^.]]*[[.]][[^.]]*[[.]]\([[0-9]][[0-9]]*\)$/\1/g'`

  if test :$MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR: = :: \
       -o :$MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR: = :: \
       -o :$MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO: = :: ;
  then
    AC_MSG_RESULT(invalid)
    AC_MSG_ERROR([nonconforming PACKAGE_VERSION='$PACKAGE_VERSION'])
  fi

  AC_DEFINE_UNQUOTED([MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR], $MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR,
                     [The major version number of this GC release.])
  AC_DEFINE_UNQUOTED([MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR], $MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR,
                     [The minor version number of this GC release.])
  AC_DEFINE_UNQUOTED([MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO], $MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO,
                     [The micro version number of this GC release.])
  AC_MSG_RESULT(major=$MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MAJOR minor=$MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MINOR \
                micro=$MANAGED_STACK_ADDRESS_BOEHM_GC_VERSION_MICRO)
])

sinclude(libtool.m4)
