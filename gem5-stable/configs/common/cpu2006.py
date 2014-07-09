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
    output_dir = '/tmp'

    if bench_name == '401.bzip2':
        bzip2 = LiveProcess()
        bzip2.executable = build_dir + '/bzip2'
        bzip2.cmd = [bzip2.executable] + [test_data + '/input/dryer.jpg', '2']
        bzip2.output = output_dir + '/dryer.jpg.out-' + fingerprint
        return (bzip2, bzip2.output, test_data + '/output/dryer.jpg.out')
    elif bench_name == '429.mcf':
        mcf = LiveProcess()
        mcf.executable = build_dir + '/mcf'
        mcf.cmd = [mcf.executable] + [test_data + '/input/inp.in']
        mcf.output = output_dir + '/inp.out-' + fingerprint
        return (mcf, mcf.output, test_data + '/output/inp.out')
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
    elif bench_name == '459.GemsFDTD':
        GemsFDTD = LiveProcess()
        GemsFDTD.executable = build_dir + '/GemsFDTD'
        GemsFDTD.cmd = [GemsFDTD.executable]
        GemsFDTD.output = '/dev/null'
        if not path.isfile('yee.dat') or not path.isfile('sphere.pec'):
           return error_input(test_data + '/input')
        return (GemsFDTD, 'sphere_td.nft',
                test_data + '/output/sphere_td.nft')
    elif bench_name == '462.libquantum':
        libquantum=LiveProcess()
        libquantum.executable = build_dir + '/libquantum'
        libquantum.cmd = [libquantum.executable] + ['33', '5']
        libquantum.output = output_dir + '/462.test.out-' + fingerprint
        return (libquantum, libquantum.output, test_data + '/output/test.out')
    elif bench_name == '470.lbm':
        lbm = LiveProcess()
        lbm.executable = build_dir + '/lbm'
        lbm.cmd = [lbm.executable] + ['20', 'reference.dat', '0', '1',
                '100_100_130_cf_a.of']
        lbm.output = output_dir + '/lbm.out-' + fingerprint
        return (lbm, lbm.output, test_data + '/output/lbm.out')
    elif bench_name == '471.omnetpp':
        omnetpp = LiveProcess()
        omnetpp.executable = build_dir + '/omnetpp'
        omnetpp.cmd = [omnetpp.executable]
        omnetpp.output = 'omnetpp.sca'
        if not path.isfile('omnetpp.ini'):
            return error_input(test_data + '/input')
        return (omnetpp, omnetpp.output, test_data + '/output/omnetpp.sca')
    elif bench_name == '483.xalancbmk':
        xalancbmk = LiveProcess()
        xalancbmk.executable = build_dir + '/Xalan'
        xalancbmk.cmd = [xalancbmk.executable, '-v'] + \
                [test_data + '/input/test.xml', test_data + '/input/xalanc.xsl']
        xalancbmk.output = output_dir + '/483.test.out-' + fingerprint
        return (xalancbmk, xalancbmk.output, test_data + '/output/test.out')
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

