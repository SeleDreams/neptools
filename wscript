# -*- mode: python -*-

# the following two variables are used by the target "waf dist"
APPNAME='neptools'

import subprocess
try:
    VERSION = subprocess.check_output(
        ['git', 'describe', '--tags', '--always'],
        stderr = subprocess.PIPE,
        universal_newlines = True).strip('\n').lstrip('v')
except:
    VERSION = '0.3.7'

def fixup_msvc():
    from waflib.TaskGen import after_method, feature
    @after_method('apply_link')
    @feature('c', 'cxx')
    def apply_flags_msvc(self):
        pass

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

try:
    with open('wscript_user.py', 'r') as f:
        exec(compile(f.read(), 'wscript_user.py', 'exec'))
except IOError:
    pass

def options(opt):
    opt.load('compiler_c compiler_cxx')
    grp = opt.get_option_group('configure options')
    grp.add_option('--clang-hack', action='store_true', default=False,
                   help='Read COMPILE.md...')
    grp.add_option('--optimize', action='store_true', default=False,
                   help='Enable some default optimizations')
    grp.add_option('--optimize-ext', action='store_true', default=False,
                   help='Optimize ext libs even if Neptools is in debug mode')
    grp.add_option('--release', action='store_true', default=False,
                   help='Enable some flags for release builds')
    grp.add_option('--overload-check', action='store_true', default=False,
                   help='Check for uncallable lua overloads (compile-time)')


    opt.add_option('--skip-run-tests', action='store_true', default=False,
                   help="Skip actually running tests with `test'")
    opt.recurse('ext')

def configure(cfg):
    from waflib import Logs
    if cfg.options.release:
        cfg.options.optimize = True

    variant = cfg.variant
    environ = cfg.environ
    # setup host compiler
    cfg.setenv(variant + '_host')
    Logs.pprint('NORMAL', 'Configuring host compiler '+variant)

    cross = False
    # replace xxx with HOST_xxx env vars
    cfg.environ = environ.copy()
    for k in list(cfg.environ):
        if k[0:5] == 'HOST_':
            cross = True
            cfg.environ[k[5:]] = cfg.environ[k]
            del cfg.environ[k]

    cfg.load('compiler_c')

    if cfg.options.optimize or cfg.options.optimize_ext:
        cfg.filter_flags_c(['CFLAGS'], ['-O1', '-march=native'])

    # ----------------------------------------------------------------------
    # setup target
    cfg.setenv(variant)
    Logs.pprint('NORMAL', 'Configuring target compiler '+variant)
    cfg.env.CROSS = cross
    cfg.environ = environ

    # override flags specific to neptools/bundled libraries
    for v in ['NEPTOOLS', 'EXT']:
        cfg.add_os_flags('CPPFLAGS_'+v, dup=False)
        cfg.add_os_flags('CFLAGS_'+v, dup=False)
        cfg.add_os_flags('CXXFLAGS_'+v, dup=False)
        cfg.add_os_flags('LINKFLAGS_'+v, dup=False)
        cfg.add_os_flags('LDFLAGS_'+v, dup=False)

    if cfg.options.clang_hack:
        cfg.find_program('clang-cl', var='CC')
        cfg.find_program('clang-cl', var='CXX')
        cfg.find_program('lld-link', var='LINK_CXX')
        cfg.find_program('llvm-lib', var='AR')
        cfg.find_program(cfg.path.abspath()+'/rc.sh', var='WINRC')

        cfg.add_os_flags('WINRCFLAGS', dup=False)
        rcflags_save = cfg.env.WINRCFLAGS

        cfg.load('msvc', funs='no_autodetect')
        fixup_msvc()
        from waflib.Tools.compiler_cxx import cxx_compiler
        from waflib.Tools.compiler_c import c_compiler
        from waflib import Utils

        plat = Utils.unversioned_sys_platform()
        cxx_save = cxx_compiler[plat]
        c_save = c_compiler[plat]
        cxx_compiler[plat] = ['msvc']
        c_compiler[plat] = ['msvc']

        cfg.env.WINRCFLAGS = rcflags_save

    cfg.load('compiler_c compiler_cxx clang_compilation_database')

    if cfg.options.clang_hack:
        cxx_compiler[plat] = cxx_save
        c_compiler[plat] = c_save

    cfg.filter_flags(['CFLAGS', 'CXXFLAGS'], [
        # error on unknown arguments, including unknown options that turns
        # unknown argument warnings into error. どうして？
        '-Werror=unknown-warning-option',
        '-Werror=ignored-optimization-argument',
        '-Werror=unknown-argument',

        '-fdiagnostics-color', '-fdiagnostics-show-option',
    ])
    cfg.filter_flags(['CFLAGS_NEPTOOLS', 'CXXFLAGS_NEPTOOLS'], [
        '-Wall', '-Wextra', '-pedantic', '-Wno-parentheses', '-Wno-assume',
        '-Wold-style-cast', '-Woverloaded-virtual', '-Wimplicit-fallthrough',
    ])
    cfg.filter_flags(['CFLAGS_EXT', 'CXXFLAGS_EXT'], [
        '-Wno-parentheses-equality', # boost fs, windows build
        '-Wno-microsoft-enum-value', '-Wno-shift-count-overflow', # ljx
        '-Wno-varargs',
    ])

    if cfg.env['COMPILER_CXX'] == 'msvc':
        cfg.define('_CRT_SECURE_NO_WARNINGS', 1)
        cfg.env.append_value('CXXFLAGS', [
            '-Xclang', '-std=c++1z',
            '-Xclang', '-fdiagnostics-format', '-Xclang', 'clang',
            '-EHsa', '-MD'])
        cfg.env.append_value('CFLAGS_EXT', '-EHsa')
        inc = '-I' + cfg.path.find_node('msvc_include').abspath()
        cfg.env.prepend_value('CFLAGS', inc)
        cfg.env.prepend_value('CXXFLAGS', inc)

        if cfg.options.optimize:
            #cfg.env.prepend_value('CFLAGS', ['/O1', '/GS-'])
            for f in ['CFLAGS_EXT', 'CXXFLAGS_EXT', 'CXXFLAGS_NEPTOOLS']:
                cfg.env.prepend_value(f, [
                    '-O2', '-Xclang', '-emit-llvm-bc'])
            cfg.env.prepend_value('LINKFLAGS', ['/OPT:REF', '/OPT:ICF'])
        elif cfg.options.optimize_ext:
            cfg.env.prepend_value('CXXFLAGS_EXT', '-O2')
    else:
        cfg.check_cxx(cxxflags='-std=c++1z')
        cfg.env.append_value('CXXFLAGS', ['-std=c++1z'])

        cfg.filter_flags(['CFLAGS', 'CXXFLAGS'], ['-fvisibility=hidden'])
        if cfg.options.optimize:
            cfg.filter_flags(['CFLAGS', 'CXXFLAGS', 'LINKFLAGS'], [
                '-Ofast', '-flto', '-fno-fat-lto-objects',
                 '-fomit-frame-pointer'])

            cfg.env.append_value('LINKFLAGS', '-Wl,-O1')
        elif cfg.options.optimize_ext:
            cfg.filter_flags(['CFLAGS_EXT', 'CXXFLAGS_EXT'], ['-g0', '-Ofast'])

    def chkdef(cfg, defn):
        return cfg.check_cxx(fragment='''
#ifndef %s
#error err
#endif
int main() { return 0; }
''' % defn,
                             msg='Checking for '+defn,
                             features='cxx',
                             mandatory=False)

    if chkdef(cfg, '_WIN64'):
        cfg.env.DEST_OS = 'win64'
    elif chkdef(cfg, '_WIN32'):
        cfg.env.DEST_OS = 'win32'

    if cfg.options.release:
        cfg.define('NDEBUG', 1)
    if cfg.env.DEST_OS == 'win32' or cfg.env.DEST_OS == 'win64':
        cfg.define('WINDOWS', 1)
        cfg.define('UNICODE', 1)
        cfg.define('_UNICODE', 1)

        cfg.check_cxx(lib='kernel32')
        cfg.check_cxx(lib='shell32')
        cfg.check_cxx(lib='user32')

    if cfg.options.overload_check:
        cfg.define('NEPTOOLS_LUA_OVERLOAD_CHECK', 1)

    Logs.pprint('NORMAL', 'Configuring ext '+variant)
    cfg.recurse('ext', name='configure', once=False)

def build_common(bld):
    fixup_msvc()
    bld.recurse('ext', name='build', once=False)

    # ignore .rc files when not on windows/no resource compiler
    from waflib.TaskGen import extension
    @extension('.rc')
    def rc_override(self, node):
        if self.env.WINRC:
            from waflib.Tools.winres import rc_file
            rc_file(self, node)

    import re
    rc_ver = re.sub('-.*', '', re.sub('\.', ',', VERSION))
    bld(features   = 'subst',
        source     = 'src/version.hpp.in',
        target     = 'src/version.hpp',
        ext_out    = ['.shit'], # otherwise every fucking c and cpp file will
                                # depend on this task...
        VERSION    = VERSION,
        RC_VERSION = rc_ver)

    src = [
        'src/except.cpp',
        'src/dumpable.cpp',
        'src/logger.cpp',
        'src/logger.lua',
        'src/low_io.cpp',
        'src/options.cpp',
        'src/pattern.cpp',
        'src/sink.cpp',
        'src/source.cpp',
        'src/txt_serializable.cpp',
        'src/utils.cpp',
        'src/format/context.cpp',
        'src/format/eof_item.cpp',
        'src/format/gbnl.cpp',
        'src/format/item.cpp',
        'src/format/raw_item.cpp',
        'src/format/cl3.cpp',
        'src/format/stcm/collection_link.cpp',
        'src/format/stcm/data.cpp',
        'src/format/stcm/exports.cpp',
        'src/format/stcm/file.cpp',
        'src/format/stcm/gbnl.cpp',
        'src/format/stcm/header.cpp',
        'src/format/stcm/instruction.cpp',
        'src/format/stsc/file.cpp',
        'src/format/stsc/header.cpp',
        'src/format/stsc/instruction.cpp',
        'src/format/stsc/string.cpp',
        'src/lua/base.cpp',
        'src/lua/base_funcs.lua',
        'src/lua/user_data.cpp',
        'src/lua/user_type.cpp',
        'src/lua/ref_counted_user_data.cpp',
    ]

    bld.stlib(source   = src,
              uselib   = 'NEPTOOLS',
              use      = 'BOOST boost_system boost_filesystem ljx',
              includes = 'src ext/ljx/src ext/brigand/include',
              target   = 'common')

def build(bld):
    build_common(bld)

    bld.program(source = 'src/programs/stcm-editor.cpp src/programs/stcm-editor.rc',
                includes = 'src ext/ljx/src ext/brigand/include', # for version.hpp
                uselib = 'NEPTOOLS',
                use = 'common',
                target = 'stcm-editor')

    if bld.env.DEST_OS == 'win32':
        # technically launcher can be compiled for 64bits, but it makes no sense
        ld = ['/nodefaultlib', '/entry:start', '/subsystem:windows', '/FIXED',
              '/NXCOMPAT:NO', '/IGNORE:4254']
        bld.program(source = 'src/programs/launcher.c src/programs/launcher.rc',
                    includes = 'src', # for version.hpp
                    target = 'launcher',
                    cflags = '-O1 -Gs9999999',
                    uselib = 'KERNEL32 SHELL32 USER32 NEPTOOLS',
                    linkflags = ld)

        # server.dll has no implib (doesn't export anything)
        from waflib.TaskGen import taskgen_method
        @taskgen_method
        def apply_implib(self):
            if getattr(self, 'defs', None) and self.env.DEST_BINFMT == 'pe':
                node = self.path.find_resource(self.defs)
                if not node:
                    raise Errors.WafError('invalid def file %r' % self.defs)

                self.env.append_value('LINKFLAGS', '/def:%s' % node.path_from(self.bld.bldnode))
                self.link_task.dep_nodes.append(node)

        src_inject = [
            'src/injected/cpk.cpp',
            'src/injected/hook.cpp',
            'src/programs/server.cpp',
            'src/programs/server.rc',
        ]
        bld.shlib(source = src_inject,
                  includes = 'src ext/brigand/include', # for version.hpp
                  target = 'neptools-server',
                  use    = 'common',
                  uselib = 'SHELL32 USER32 NEPTOOLS',
                  defs   = 'src/programs/server.def')

from waflib.Build import BuildContext
class TestContext(BuildContext):
    cmd = 'test'
    fun = 'test'

def test(bld):
    build_common(bld)

    # feature fail_cxx: expect compilation failure
    import sys
    from waflib import Logs
    from waflib.Task import Task
    from waflib.TaskGen import extension, feature
    from waflib.Tools import c_preproc
    @extension('.cpp')
    def fail_cxx_ext(self, node):
        if 'fail_cxx' in self.features:
            return self.create_compiled_task('fail_cxx', node)
        else:
            return self.create_compiled_task('cxx', node)

    class fail_cxx(Task):
        #run_str = '${CXX} ${ARCH_ST:ARCH} ${CXXFLAGS} ${CPPFLAGS} ${FRAMEWORKPATH_ST:FRAMEWORKPATH} ${CPPPATH_ST:INCPATHS} ${DEFINES_ST:DEFINES} ${CXX_SRC_F}${SRC} ${CXX_TGT_F}${TGT[0].abspath()}'
        def run(tsk):
            env = tsk.env
            gen = tsk.generator
            bld = gen.bld
            cwdx = getattr(bld, 'cwdx', bld.bldnode) # TODO single cwd value in waf 1.9
            wd = getattr(tsk, 'cwd', None)
            def to_list(xx):
                if isinstance(xx, str): return [xx]
                return xx
            tsk.last_cmd = lst = []
            lst.extend(to_list(env['CXX']))
            lst.extend(tsk.colon('ARCH_ST', 'ARCH'))
            lst.extend(to_list(env['CXXFLAGS']))
            lst.extend(to_list(env['CPPFLAGS']))
            lst.extend(tsk.colon('FRAMEWORKPATH_ST', 'FRAMEWORKPATH'))
            lst.extend(tsk.colon('CPPPATH_ST', 'INCPATHS'))
            lst.extend(tsk.colon('DEFINES_ST', 'DEFINES'))
            lst.extend(to_list(env['CXX_SRC_F']))
            lst.extend([a.path_from(cwdx) for a in tsk.inputs])
            lst.extend(to_list(env['CXX_TGT_F']))
            lst.append(tsk.outputs[0].abspath())
            lst = [x for x in lst if x]
            try:
                (out,err) = bld.cmd_and_log(
                    lst, cwd=bld.variant_dir, env=env.env or None,
                    quiet=0, output=0)
                Logs.error("%s compiled successfully, but it shouldn't" % tsk.inputs[0])
                Logs.info(out, extra={'stream':sys.stdout, 'c1': ''})
                Logs.info(err, extra={'stream':sys.stderr, 'c1': ''})
                return -1
            except Exception as e:
                # create output to silence missing file errors
                open(tsk.outputs[0].abspath(), 'w').close()
                return 0

        vars    = ['CXXDEPS'] # unused variable to depend on, just in case
        ext_in  = ['.h'] # set the build order easily by using ext_out=['.h']
        scan    = c_preproc.scan

    # test pattern parser
    bld(features='cxx',
        source='test/pattern_fail.cpp',
        uselib='BOOST NEPTOOLS',
        use='boost_system boost_filesystem',
        includes='src ext/brigand/include',
        defines=['TEST_PATTERN="b2 ff"'])
    bld(features='cxx fail_cxx',
        source='test/pattern_fail.cpp',
        uselib='BOOST NEPTOOLS',
        use='boost_system boost_filesystem',
        includes='src ext/brigand/include',
        defines=['TEST_PATTERN="bz ff"'])

    src = [
        'test/main.cpp',
        'test/options.cpp',
        'test/pattern.cpp',
        'test/sink.cpp',
        'test/container/ordered_map.cpp',
        'test/container/parent_list.cpp',
        'test/lua/base.cpp',
        'test/lua/function_call.cpp',
        'test/lua/function_ref.cpp',
        'test/lua/user_type.cpp',
    ]
    bld.program(source   = src,
                includes = 'src ext/catch/include ext/brigand/include',
                uselib   = 'NEPTOOLS',
                use      = 'common',
                target   = 'run-tests')

    if not bld.options.skip_run_tests:
        bld.add_post_fun(lambda ctx:
            ctx.exec_command([
                bld.bldnode.find_or_declare('run-tests').abspath(),
                '--use-colour', 'yes'], cwd=bld.variant_dir) == 0 or ctx.fatal('Test failure')
        )

from waflib.Configure import conf
@conf
def filter_flags(cfg, vars, flags):
    ret = []

    for flag in flags:
        try:
            # gcc ignores unknown -Wno-foo flags but not -Wfoo, but warns if
            # there are other warnings. with clang, just depend on
            # -Werror=ignored-*-option, -Werror=unknown-*-option
            testflag = flag
            if flag[0:5] == '-Wno-':
                testflag = '-W'+flag[5:]
            cfg.check_cxx(cxxflags=[testflag], features='cxx',
                          msg='Checking for compiler flags '+testflag)
            ret.append(flag)
            for var in vars:
                cfg.env.append_value(var, flag)
        except:
            pass

    return ret

@conf
def filter_flags_c(cfg, vars, flags):
    ret = []

    for flag in flags:
        try:
            # gcc ignores unknown -Wno-foo flags but not -Wfoo, but warns if
            # there are other warnings. with clang, just depend on
            # -Werror=ignored-*-option, -Werror=unknown-*-option
            testflag = flag
            if flag[0:5] == '-Wno-':
                testflag = '-W'+flag[5:]
            cfg.check_cc(cflags=[testflag],
                         msg='Checking for compiler flags '+testflag)
            ret.append(flag)
            for var in vars:
                cfg.env.append_value(var, flag)
        except:
            pass

    return ret

# waf 1.7 style logging: c/cxx instead of vague shit like 'processing',
# 'compiling', etc.
from waflib.Task import Task
def getname(self):
    return self.__class__.__name__ + ':'
Task.keyword = getname
