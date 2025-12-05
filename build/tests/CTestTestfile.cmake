# CMake generated Testfile for 
# Source directory: /home/haaken/github-projects/gr-sleipnir/gr-opus/tests
# Build directory: /home/haaken/github-projects/gr-sleipnir/gr-opus/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(qa_opus_encoder "/usr/bin/python3" "-B" "/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/qa_opus_encoder.py")
set_tests_properties(qa_opus_encoder PROPERTIES  _BACKTRACE_TRIPLES "/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/CMakeLists.txt;21;add_test;/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/CMakeLists.txt;0;")
add_test(qa_opus_decoder "/usr/bin/python3" "-B" "/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/qa_opus_decoder.py")
set_tests_properties(qa_opus_decoder PROPERTIES  _BACKTRACE_TRIPLES "/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/CMakeLists.txt;22;add_test;/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/CMakeLists.txt;0;")
add_test(qa_opus_roundtrip "/usr/bin/python3" "-B" "/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/qa_opus_roundtrip.py")
set_tests_properties(qa_opus_roundtrip PROPERTIES  _BACKTRACE_TRIPLES "/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/CMakeLists.txt;23;add_test;/home/haaken/github-projects/gr-sleipnir/gr-opus/tests/CMakeLists.txt;0;")
