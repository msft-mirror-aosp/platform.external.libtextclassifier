find src -name "*.java" -exec google-java-format -r -a {} \;
find tests/unittests/src -name "*.java" -exec google-java-format -r -a {} \;
find tests/robotests/src -name "*.java" -exec google-java-format -r -a {} \;