cd ../..

# Generate forward declarations
# JHU 22-Apr-2019 disable until Xpetra's conversion to ETI is complete.
#cd ./Utils/ForwardDeclaration/
#./gen_fwdFiles.sh
#cd ../..

# Generate explicit instantiation
cd ./Utils/ExplicitInstantiation/
# generate the ETI_LO_GO_NO_classes.cmake and
# ETI_SC_LO_GO_NO_classes.cmake files for auto
# generating the ETI *.cpp files in  Xpetra
./genClassLists.sh
cd ../..

# Touch CMakeLists.txt for a new class
new=`git status -s ./Utils/ForwardDeclaration/ ./Utils/ExplicitInstantiation/ | grep "^??" | wc -l`
if [ "$new" != "0" ]; then
  echo "# touch CMakeLists.txt because a new file was created in Utils/ExplicitInstantiation of Utils/ForwardDeclaration" >> CMakeLists.txt
fi
