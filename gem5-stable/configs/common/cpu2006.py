from os import path
from m5.objects import *

# Build SPEC CPU 2006 
# (1) Do not forget to install gfortran, in addition to gcc.
# (2) Create default.cfg in config directory, so that no --config option is necessary for runspec. 
# (3) In default.cfg, append "-static" to the end of the following lines: 
# COPTIMIZE    = -O2 -fno-strict-aliasing -static 
# CXXOPTIMIZE  = -O2 -fno-strict-aliasing -static 
# FOPTIMIZE    = -O2 -fno-strict-aliasing -static 
# (4) Run "runspec -a build all" after ". ./shrc"

def error_input(input_dir):
    print >> sys.stderr, 'Files in ' + input_dir + \
            ' must be copied to the current working directory.'
    return (None, None, None)

def make_process(bench_name, bench_root, build_name, fingerprint):
    bench_dir = bench_root + '/' + bench_name
    build_dir = bench_dir + '/build/' + build_name
    test_data = bench_dir + '/data/test'
    ref_data = bench_dir + '/data/ref'
    output_dir = '/tmp'

    if bench_name == '401.bzip2':
        bzip2 = LiveProcess()
        bzip2.executable = build_dir + '/bzip2'
        bzip2.cmd = [bzip2.executable] + [test_data + '/input/dryer.jpg', '2']
        bzip2.output = output_dir + '/dryer.jpg.out-' + fingerprint
        return (bzip2, bzip2.output, test_data + '/output/dryer.jpg.out')
    elif bench_name == '403.gcc':
        gcc = LiveProcess()
        gcc.executable = build_dir + '/gcc'
        gcc.cmd = [gcc.executable] + [ref_data + '/input/166.i']
        gcc.output = output_dir + '/403.gcc.out-' + fingerprint
        return (gcc, gcc.output, ref_data + '/output/166.s')
    elif bench_name == '410.bwaves':
        bwaves = LiveProcess()
        bwaves.executable = build_dir + '/bwaves'
        bwaves.cmd = [bwaves.executable]
        bwaves.output = output_dir + '/410.bwaves.out-' + fingerprint
        return (bwaves, bwaves.output, ref_data + '/output/bwaves.out')
    elif bench_name == '429.mcf':
        mcf = LiveProcess()
        mcf.executable = build_dir + '/mcf'
        mcf.cmd = [mcf.executable] + [ref_data + '/input/inp.in']
        mcf.output = output_dir + '/inp.out-' + fingerprint
        return (mcf, mcf.output, ref_data + '/output/inp.out')
    elif bench_name == '433.milc':
        milc = LiveProcess()
        milc.executable = build_dir + '/milc'
        milc.cmd = [milc.executable]
        milc.input = ref_data + '/input/su3imp.in'
        milc.output = output_dir + '/433.milc.out-' + fingerprint
        return (milc, milc.output, ref_data + '/output/su3imp.out')
    elif bench_name == '437.leslie3d':
        leslie3d = LiveProcess()
        leslie3d.executable = build_dir + '/leslie3d'
        leslie3d.cmd = [leslie3d.executable]
        leslie3d.input = ref_data + '/input/leslie3d.in'
        leslie3d.output = output_dir + '/437.leslie3d.out-' + fingerprint
        return (leslie3d, leslie3d.output, ref_data + '/output/leslie3d.out')
    elif bench_name == '453.povray':
        povray = LiveProcess()
        povray.executable = build_dir + '/povray'
        povray.cmd = [povray.executable] + \
                [test_data + '/input/SPEC-benchmark-test.ini'] + \
                ['+L', bench_dir + '/data/all/input/']
        povray.output = '/dev/null'
        if not path.isfile('SPEC-benchmark-test.pov'):
            return error_input(test_data + '/input') 
        return (povray, 'SPEC-benchmark.tga',
                test_data + '/output/SPEC-benchmark.tga')
    elif bench_name == '454.calculix':
        calculix = LiveProcess()
        calculix.executable = build_dir + '/calculix'
        calculix.cmd = [calculix.executable] + \
                ['-i', test_data + '/input/beampic']
        calculix.output = '/dev/null'
        return (calculix, test_data + '/input/beampic.dat',
                test_data + '/output/beampic.dat')
    elif bench_name == '456.hmmer':
        hmmer = LiveProcess()
        hmmer.executable = build_dir + '/hmmer'
        input_file = test_data + '/input/bombesin.hmm'
        hmmer.cmd = [hmmer.executable] + ['--fixed', '0', '--mean', '325',
                '--num', '45000', '--sd', '200', '--seed', '0', input_file]
        hmmer.output = output_dir + '/bombesin.out-' + fingerprint
        return (hmmer, hmmer.output, test_data + '/output/bombesin.out')
    elif bench_name == '458.sjeng':
        sjeng = LiveProcess()
        sjeng.executable = build_dir + '/sjeng'
        sjeng.cmd = [sjeng.executable] + [test_data + '/input/test.txt']
        sjeng.output = output_dir + '/test.out-' + fingerprint
        return (sjeng, sjeng.output, test_data + '/output/test.out')
    elif bench_name == '450.soplex':
        soplex = LiveProcess()
        soplex.executable = build_dir + '/soplex'
        soplex.cmd = [soplex.executable] + ['-s1', '-e', '-m45000',
                ref_data + '/input/pds-50.mps']
        soplex.output = output_dir + '/450.soplex.out-' + fingerprint
        return (soplex, soplex.output, ref_data + '/output/pds-50.mps.out')
    elif bench_name == '459.GemsFDTD':
        GemsFDTD = LiveProcess()
        GemsFDTD.executable = build_dir + '/GemsFDTD'
        GemsFDTD.cmd = [GemsFDTD.executable]
        GemsFDTD.output = '/dev/null'
        if not path.isfile('yee.dat') or not path.isfile('sphere.pec'):
           return error_input(ref_data + '/input')
        return (GemsFDTD, 'sphere_td.nft',
                ref_data + '/output/sphere_td.nft')
    elif bench_name == '462.libquantum':
        libquantum=LiveProcess()
        libquantum.executable = build_dir + '/libquantum'
        libquantum.cmd = [libquantum.executable] + ['1397', '8']
        libquantum.output = output_dir + '/462.ref.out-' + fingerprint
        return (libquantum, libquantum.output, ref_data + '/output/ref.out')
    elif bench_name == '470.lbm':
        lbm = LiveProcess()
        lbm.executable = build_dir + '/lbm'
        lbm.cmd = [lbm.executable] + ['3000', 'reference.dat', '0', '0',
                ref_data + '/input/100_100_130_ldc.of']
        lbm.output = output_dir + '/lbm.out-' + fingerprint
        return (lbm, lbm.output, ref_data + '/output/lbm.out')
    elif bench_name == '471.omnetpp':
        omnetpp = LiveProcess()
        omnetpp.executable = build_dir + '/omnetpp'
        omnetpp.cmd = [omnetpp.executable, 'omnetpp.ini']
        omnetpp.output = 'omnetpp.sca'
        if not path.isfile('omnetpp.ini'):
            return error_input(ref_data + '/input')
        return (omnetpp, omnetpp.output, ref_data + '/output/omnetpp.sca')
    elif bench_name == '473.astar':
        astar = LiveProcess()
        astar.executable = build_dir + '/astar'
        astar.cmd = [astar.executable] + \
                [ref_data + '/input/BigLakes2048.cfg']
        astar.output = output_dir + '/473.ref.out-' + fingerprint
        if not path.isfile('BigLakes2048.bin'):
            return error_input(ref_data + '/input')
        return (astar, astar.output,
                ref_data + '/output/BigLakes2048.out') 
    elif bench_name == '483.xalancbmk':
        xalancbmk = LiveProcess()
        xalancbmk.executable = build_dir + '/Xalan'
        xalancbmk.cmd = [xalancbmk.executable, '-v'] + \
                [ref_data + '/input/t5.xml', ref_data + '/input/xalanc.xsl']
        xalancbmk.output = output_dir + '/483.ref.out-' + fingerprint
        return (xalancbmk, xalancbmk.output, ref_data + '/output/ref.out')
    elif bench_name == '998.specrand':
        specrand_i = LiveProcess()
        specrand_i.executable = build_dir + '/specrand'
        specrand_i.cmd = [specrand_i.executable] + ['324342', '24239']
        specrand_i.output = output_dir + '/rand.24239.out-' + fingerprint
        return (specrand_i, specrand_i.output,
                test_data + '/output/rand.24239.out')
    elif bench_name == '999.specrand':
        specrand_f = LiveProcess()
        specrand_f.executable = build_dir + '/specrand'
        specrand_f.cmd = [specrand_f.executable] + ['324342','24239']
        specrand_f.output = output_dir + '/rand.24239.out-' + fingerprint
        return (specrand_f, specrand_f.output,
                test_data + '/output/rand.24239.out')
    else:
        print >> sys.stderr, '[Error] make_process() does not recognize ' + \
                bench_name
        return (None, None, None)

