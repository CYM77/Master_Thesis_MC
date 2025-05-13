## My 3D-DRAM model using Ramulator2
- After building the project using cmake and make -j8, uses scripts to generate patterns
- The yaml config files has all the files needed to run
- Debugger usage can be set using .vscode folder settings



./00_setup.tcl
cd build/
cmake ..
make -j8

cd ../scripts
python3 latency_verification_pattern.py(生成trace檔案)
python3 ld_st_trace_gen.py

cd ../build
./ramulator2 -f ../Trace_Verification_RTL_C++/testimg_latency_verification.......yaml(其中一檔案)




