clear;

echo ""
echo "------------------------------------------------------------------------"
echo "----------------------------  RECOGNIZER  ------------------------------"
echo "------------------------------------------------------------------------"
ps aux | grep recognizer ; 
echo ""
killall -9 recognizer; 
echo ""
ps aux | grep recognizer

echo ""
echo "------------------------------------------------------------------------"
echo "------------------------------  TRACKER  -------------------------------"
echo "------------------------------------------------------------------------"
ps aux | grep tracker ; 
echo ""
killall -9 tracker; 
echo ""
echo "killed TRACKERs" ; 
ps aux | grep tracker

echo "------------------------------------------------------------------------"
echo "-------------------------------  JAVA  ---------------------------------"
echo "------------------------------------------------------------------------"
ps aux | grep java ; 
echo ""
killall -9 java; 
echo ""
echo "killed JAVAs" ; 
ps aux | grep java

echo "------------------------------------------------------------------------"
