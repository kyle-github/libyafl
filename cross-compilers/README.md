# Cross-Compiler Toolchains

This directory contains pre-built cross-compiler toolchains from [musl.cc](https://musl.cc/).

## Files

These files are stored with Git LFS (Large File Storage) and are downloaded via GitHub Actions CI/CD:

- `arm-linux-musleabihf-cross.tgz` - ARM 32-bit (ARMv7) EABI Hard Float
- `riscv64-linux-musl-cross.tgz` - RISC-V 64-bit
- `s390x-linux-musl-cross.tgz` - IBM System z (s390x)
- `powerpc64le-linux-musl-cross.tgz` - PowerPC 64-bit Little Endian

## Downloading Toolchains Locally

To download these toolchains to your local machine:

1. **Install Git LFS** (if not already installed):
   ```bash
   # macOS
   brew install git-lfs

   # Ubuntu/Debian
   sudo apt-get install git-lfs

   # Other systems: https://git-lfs.com/
   ```

2. **Initialize Git LFS in this repository** (one-time setup):
   ```bash
   git lfs install
   ```

3. **Run the download script**:
   ```bash
   ./scripts/download-musl-cc.sh
   ```

   This script will:
   - Create this directory if it doesn't exist
   - Download all four cross-compiler toolchains
   - Retry up to 3 times if downloads fail
   - Store them for Git LFS tracking

4. **Commit and push to GitHub**:
   ```bash
   git add cross-compilers/ .gitattributes
   git commit -m "Add musl.cc cross-compiler toolchains"
   git push
   ```

## Git LFS Tracking

These files are tracked with Git LFS, which means:

- Git stores pointers to the actual files instead of the full archives
- The repository size remains small
- When you clone or pull, Git LFS downloads only the files you need
- Bandwidth usage is optimized for large binary files

To verify Git LFS is working correctly, check the `.gitattributes` file to see which patterns are tracked.

## Why These Toolchains?

The musl.cc project provides pre-built, statically-linked cross-compiler toolchains. They're:
- Ready to use without system dependencies
- Statically compiled (portable across Linux distributions)
- Designed for cross-compilation to various architectures
- Updated regularly with latest compiler versions

## Updates

To update these toolchains to newer versions in the future:

1. Run the download script again: `./scripts/download-musl-cc.sh`
2. Git will detect changes if new versions are available
3. Commit and push the updated files

## References

- [musl.cc](https://musl.cc/) - Pre-built musl libc cross-compiler toolchains
- [Git LFS Documentation](https://git-lfs.com/)
