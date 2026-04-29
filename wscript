import os.path
top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')

def build(ctx):
    ctx.load('pebble_sdk')
    build_worker = os.path.exists('worker_src')
    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[platform])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pebble_build_group(
            build_worker=build_worker,
            sources=ctx.path.ant_glob('src/c/**/*.c'),
            libraries=['m'],
        )
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.set_group('bundle')
    ctx.pebble_bundle(binaries=binaries)
