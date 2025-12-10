# Quick Build & Run Guide

## 🚀 Fast Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install libgtk-3-dev libglib2.0-dev gcc make pkg-config

# Fedora
sudo dnf install gtk3-devel glib2-devel gcc make pkg-config

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gcc make

# macOS
brew install gtk+3 pkg-config
```

## 🔨 Build

### Option 1: Using Makefile (Recommended)
```bash
cd c:\Users\edoua\Documents\GitHub\s1OCR\interface
make clean
make
```

### Option 2: Manual GCC (if Makefile missing)
```bash
cd c:\Users\edoua\Documents\GitHub\s1OCR\interface
gcc -std=c99 -Wall -o ocr \
  main.c ocr_window.c \
  ../nn/nn_c.c \
  ../binary/binary_api.c \
  $(pkg-config --cflags --libs gtk+-3.0 glib-2.0) \
  -I.. -lm
```

### Option 3: PowerShell (Windows)
```powershell
cd "c:\Users\edoua\Documents\GitHub\s1OCR\interface"
gcc -std=c99 -Wall -o ocr.exe `
  main.c ocr_window.c `
  ..\nn\nn_c.c `
  ..\binary\binary_api.c `
  $(pkg-config --cflags --libs gtk+-3.0 glib-2.0) `
  -I.. -lm
```

## ▶️ Run

```bash
# Navigate to interface directory
cd interface/

# Execute
./ocr          # Linux/macOS
ocr.exe        # Windows
```

## 📋 Files Needed

Before compiling, ensure these exist:

```
✅ interface/
   ├── main.c (exists)
   ├── ocr_window.c (UPDATED - 929 lines)
   ├── ocr_window.h (exists)
   └── Makefile (should exist)

✅ nn/
   ├── nn_ocr.h (CREATED)
   ├── nn_c.c (UPDATED)
   └── weights.txt (OPTIONAL - create or train)

✅ binary/
   ├── binary_api.h (CREATED)
   ├── binary_api.c (CREATED)
   ├── stb_image.h (exists)
   ├── stb_image_write.h (exists)
   └── Makefile (exists)

✅ solver/
   ├── solver.h (exists)
   ├── solver.c (exists)
   └── Makefile (exists)
```

## 🏃 Troubleshooting

### Error: "gtk/gtk.h: No such file or directory"
**Solution**: Install GTK3 development files
```bash
# Ubuntu
sudo apt-get install libgtk-3-dev

# Fedora  
sudo dnf install gtk3-devel
```

### Error: "pkg-config not found"
**Solution**: Install pkg-config
```bash
sudo apt-get install pkg-config      # Ubuntu
sudo dnf install pkg-config           # Fedora
brew install pkg-config               # macOS
```

### Error: "nn_ocr.h: No such file"
**Solution**: Ensure you're in `interface/` directory and paths are correct
```bash
# Check file exists
ls ../nn/nn_ocr.h      # Should exist
```

### Error: "stb_image.h: No such file"
**Solution**: Verify binary folder structure
```bash
ls ../binary/stb_image.h
```

### Linker Error: "undefined reference to `nn_init`"
**Solution**: Make sure to compile `nn_c.c`:
```bash
gcc ... ocr_window.c ../nn/nn_c.c ../binary/binary_api.c ...
```

## ✅ Test Compilation

Quick test (no linking):
```bash
gcc -c -std=c99 -Wall ocr_window.c $(pkg-config --cflags gtk+-3.0 glib-2.0) -I..
# Should produce ocr_window.o
```

## 📦 Creating Weights File

If `nn/weights.txt` doesn't exist:

### Option 1: Use Python training script
```bash
cd nn/
python train_nn.py
# Creates: weights.txt
```

### Option 2: Auto-initialize in app
- App will initialize random weights if file missing
- NN will work but be untrained

### Option 3: Manual format
```
784 128 26
0.1 0.1 0.1 ... (784*128 values)
0.0 0.0 0.0 ... (128 values)
0.1 0.1 0.1 ... (128*26 values)
0.0 0.0 0.0 ... (26 values)
```

## 🎮 Running the App

```
1. Execute: ./ocr
2. Click [Entrer] to enter main interface
3. Click [📁 Open] to load image
4. Try buttons:
   - [▶ Auto] for rotation carousel
   - [⚫ Bin] for binarization
   - [📊 Extract] for letter extraction
   - [🔤 OCR] for recognition
   - [✓ Solve] for solving (placeholder)
```

## 📁 Directory Structure Check

```bash
# From s1OCR root:
tree -L 2

s1OCR/
├── interface/
│   ├── main.c
│   ├── ocr_window.c (929 lines)
│   ├── ocr_window.h
│   ├── Makefile
│   └── samples/
├── nn/
│   ├── nn_ocr.h
│   ├── nn_c.c
│   ├── weights.txt
│   └── ...
├── binary/
│   ├── binary_api.h
│   ├── binary_api.c
│   ├── stb_image.h
│   ├── stb_image_write.h
│   └── Makefile
├── solver/
│   ├── solver.h
│   ├── solver.c
│   └── ...
└── INTEGRATION_SUMMARY.md
```

## 🔍 Verify Build Works

After compilation:
```bash
# Check executable exists
ls -la ocr
# Should show: -rwxr-xr-x ... ocr

# Run with debug info
./ocr 2>&1 | head -20
# Should show GTK initialization

# Check dependencies
ldd ./ocr | grep gtk    # Linux
otool -L ./ocr | grep gtk  # macOS
```

## 💡 Tips

- **First build**: May take 1-2 minutes if GTK not cached
- **Incremental builds**: Only modified files recompiled
- **Debug build**: Add `-g` flag: `gcc -g ... `
- **Optimization**: Add `-O2` flag: `gcc -O2 ... `
- **Strip binary**: `strip ocr` to reduce size

## 📚 Documentation Files Created

- `INTEGRATION_SUMMARY.md` - Full system overview
- `INTERFACE_VISUAL_GUIDE.md` - UI/UX walkthroughs  
- `QUICK_BUILD_GUIDE.md` - This file

---

**Status**: ✅ Ready to compile
**Lines of Code**: ~930 (interface) + 230 (nn) + 220 (binary) = ~1,380 total
**Features**: Auto-rotation, binarization, OCR, solver-ready, full GTK integration
