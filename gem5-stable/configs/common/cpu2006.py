from m5.objects import *

def make_process(bench_name, bench_root):
    bench_dir = bench_root + '/' + bench_name
    test_data = bench_dir + '/data/test'
    output_dir = '/tmp'

    if bench_name == '483.xalancbmk':
        xalancbmk = LiveProcess()
        xalancbmk.executable = bench_dir + '/Xalan'
        xalancbmk.cmd = [xalancbmk.executable, '-v'] + \
                [test_data + '/input/test.xml', test_data + '/input/xalanc.xsl']
        xalancbmk.output = output_dir + '/test.out'
        return (xalancbmk, test_data + '/output/test.out')
    elif bench_name == '998.specrand':
        specrand_i = LiveProcess()
        specrand_i.executable = bench_dir + '/specrand'
        specrand_i.cmd = [specrand_i.executable] + ['324342', '24239']
        specrand_i.output = output_dir + '/rand.24239.out'
        return (specrand_i, test_data + '/output/rand.24239.out')
    elif bench_name == '999.specrand':
        specrand_f = LiveProcess()
        specrand_f.executable = bench_dir + '/specrand'
        specrand_f.cmd = [specrand_f.executable] + ['324342','24239']
        specrand_f.output = output_dir + '/rand.24239.out'
        return (specrand_f, test_data + '/output/rand.24239.out')
    else:
        print >> sys.stderr, '[Error] make_process() does not recognize ' + \
                bench_name
        return (None, None)
