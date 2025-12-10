# 📋 API Quick Reference Card

## Neural Network API (nn_ocr.h)

### `int nn_init(const char *weights_path);`
**Purpose**: Initialize neural network and load weights
- **Input**: Path to weights file (e.g., "nn/weights.txt")
- **Returns**: 1 on success, 0 on failure
- **Side effects**: Allocates W1, b1, W2, b2, h, z2, a2 buffers
- **Called from**: `ocr_app_window_init()`
- **Example**: 
  ```c
  if (!nn_init("nn/weights.txt")) {
    g_critical("Failed to init NN");
  }
  ```

### `char nn_predict_letter_from_file(const char *png_path);`
**Purpose**: Recognize single letter from PNG image
- **Input**: Path to 28×28 PNG letter image
- **Returns**: 'A'-'Z' on success, '?' if prediction < 0.5 confidence
- **Side effects**: Loads PNG from disk, processes through NN
- **Called from**: `on_ocr_clicked()`
- **Time**: ~5-10ms per letter
- **Example**:
  ```c
  char letter = nn_predict_letter_from_file("letters/letter_0000.png");
  if (letter >= 'A' && letter <= 'Z') {
    g_string_append_c(grid, letter);
  }
  ```

### `int nn_process_grid(const char *letters_dir, const char *grille_path, const char *mots_path);`
**Purpose**: Batch process all letters in folder (alternative to individual calls)
- **Input**: 
  - `letters_dir`: Folder with letter PNG files
  - `grille_path`: Output file for space-separated grid
  - `mots_path`: Output file for grid text (no spaces)
- **Returns**: 1 on success, 0 on failure
- **Side effects**: Writes two output files
- **Called from**: Not yet used (alternative approach)
- **Example**:
  ```c
  nn_process_grid("letters", "out/grille.txt", "out/grid_text.txt");
  ```

### `void nn_shutdown(void);`
**Purpose**: Free all allocated buffers
- **Input**: None
- **Returns**: void
- **Side effects**: Frees W1, b1, W2, b2, h, z2, a2
- **Called from**: App shutdown (future - optional)
- **Example**:
  ```c
  g_signal_connect(app, "shutdown", G_CALLBACK(on_app_shutdown), NULL);
  ```

---

## Image Processing API (binary_api.h)

### Data Structures

#### `BinaryImage`
```c
typedef struct {
  uint8_t *data;        // Pixel array (0 or 255)
  int width;            // Image width in pixels
  int height;           // Image height in pixels
} BinaryImage;
```

#### `BoundingBox`
```c
typedef struct {
  int x, y, w, h;       // x,y = top-left; w,h = dimensions
} BoundingBox;
```

#### `ComponentList`
```c
typedef struct {
  BoundingBox *boxes;   // Array of bounding boxes
  int count;            // Number of components
} ComponentList;
```

### Functions

### `BinaryImage* binary_load_otsu(const char *png_path);`
**Purpose**: Load PNG image and binarize using Otsu's method
- **Input**: Path to PNG file
- **Returns**: Allocated BinaryImage pointer (or NULL on error)
- **Side effects**: Allocates image data buffer
- **Time**: ~50-100ms depending on image size
- **Called from**: `on_binarize_clicked()`
- **Memory**: Caller must `free()` the returned pointer
- **Example**:
  ```c
  BinaryImage *bin = binary_load_otsu("image.png");
  if (!bin) {
    g_warning("Failed to load image");
    return;
  }
  self->current_binary = bin;
  ```

### `void binary_free(BinaryImage *img);`
**Purpose**: Free allocated binary image
- **Input**: Pointer to BinaryImage
- **Returns**: void
- **Side effects**: Frees `img->data` and `img`
- **Note**: Always pair with `binary_load_otsu()`
- **Example**:
  ```c
  if (self->current_binary) {
    binary_free(self->current_binary);
    self->current_binary = NULL;
  }
  ```

### `int binary_save_png(const BinaryImage *img, const char *output_path);`
**Purpose**: Save binary image to PNG file
- **Input**: 
  - `img`: Binary image to save
  - `output_path`: Output PNG filename
- **Returns**: 1 on success, 0 on failure
- **Side effects**: Writes PNG file to disk
- **Time**: ~20-50ms depending on size
- **Called from**: `on_binarize_clicked()`, letter extraction
- **Example**:
  ```c
  if (!binary_save_png(bin, "out/binarized.png")) {
    g_error("Failed to save image");
  }
  ```

### `ComponentList* binary_find_components(const BinaryImage *img);`
**Purpose**: Find connected components (letters) in binary image
- **Input**: Binary image to analyze
- **Returns**: Allocated ComponentList (or NULL on error)
- **Side effects**: Allocates ComponentList and boxes array
- **Algorithm**: Flood-fill (8-connected)
- **Time**: ~50-200ms depending on component count
- **Called from**: `on_extract_grid_clicked()`
- **Memory**: Caller must `binary_free_components()`
- **Example**:
  ```c
  ComponentList *comps = binary_find_components(bin);
  if (!comps) return;
  
  g_print("Found %d components\n", comps->count);
  for (int i = 0; i < comps->count; i++) {
    BoundingBox box = comps->boxes[i];
    g_print("Component %d: (%d,%d) %dx%d\n", i, box.x, box.y, box.w, box.h);
  }
  binary_free_components(comps);
  ```

### `void binary_free_components(ComponentList *comp);`
**Purpose**: Free allocated component list
- **Input**: Pointer to ComponentList
- **Returns**: void
- **Side effects**: Frees `comp->boxes` and `comp`
- **Note**: Always pair with `binary_find_components()`
- **Example**:
  ```c
  binary_free_components(comps);
  ```

### `BinaryImage* binary_crop(const BinaryImage *img, const BoundingBox *box);`
**Purpose**: Extract sub-region from binary image
- **Input**:
  - `img`: Source binary image
  - `box`: Bounding box defining region to crop
- **Returns**: New BinaryImage with cropped region
- **Side effects**: Allocates new image data
- **Called from**: `on_extract_grid_clicked()` (for each component)
- **Memory**: Caller must `binary_free()`
- **Example**:
  ```c
  BoundingBox letter_box = comps->boxes[i];
  BinaryImage *letter = binary_crop(bin, &letter_box);
  if (letter) {
    binary_save_png(letter, "letters/letter_0000.png");
    binary_free(letter);
  }
  ```

---

## GTK Interface (ocr_window.c)

### Struct Fields (OcrAppWindow)

#### Rotation & Auto-Rotation
```c
GPtrArray *samples;              // Array of sample file paths
guint auto_rotate_timeout;       // GLib timeout ID (0 = inactive)
int sample_index;                // Current sample in array
gdouble auto_angle;              // Current rotation (0-360°)
gboolean auto_rotation_enabled;  // Is auto-rotation active?
GtkWidget *auto_rotate_btn;      // Reference to button
```

#### Image & Processing
```c
BinaryImage *current_binary;     // Current binarized image (or NULL)
GtkWidget *image_widget;         // Image display widget
GtkWidget *scroller;             // Scrolled window container
GtkWidget *status_label;         // Status feedback label
GtkWidget *angle_spin;           // Rotation angle spinner
```

#### Results & Text
```c
GtkWidget *grid_text_view;       // Display OCR'd grid text
GtkWidget *words_text_view;      // Words input area (future)
GtkWidget *results_text_view;    // Solver results display
gchar *current_grid;             // Recognized grid as string
unsigned int grid_rows, grid_cols;  // Grid dimensions
```

### Callbacks

#### `on_binarize_clicked(GtkButton *button, gpointer user_data)`
**Triggered by**: [⚫ Bin] button
**Flow**:
1. Get current image path
2. Call `binary_load_otsu()`
3. Store in `self->current_binary`
4. Call `binary_save_png()` to "out/binarized.png"
5. Display with `gtk_image_set_from_file()`
6. Update status label

#### `on_extract_grid_clicked(GtkButton *button, gpointer user_data)`
**Triggered by**: [📊 Extract] button
**Flow**:
1. Check `self->current_binary` exists
2. Call `binary_find_components()`
3. For each component:
   - Call `binary_crop()`
   - Resize to 28×28 nearest-neighbor
   - Save to `letters/letter_XXXX.png`
4. Update status with count

#### `on_ocr_clicked(GtkButton *button, gpointer user_data)`
**Triggered by**: [🔤 OCR] button
**Flow**:
1. Open `letters/` folder
2. For each `.png` file:
   - Call `nn_predict_letter_from_file()`
   - Append result to grid string
3. Store in `self->current_grid`
4. Display in `grid_text_view`
5. Update status with letter count

#### `on_solve_clicked(GtkButton *button, gpointer user_data)`
**Triggered by**: [✓ Solve] button
**Status**: Currently placeholder
**Future**:
1. Parse `self->current_grid` into 2D array
2. Read words from `solver/grid/words.txt`
3. For each word:
   - Call `solver_search_word()`
   - Get (row, col, direction)
4. Display results in `results_text_view`

#### `on_auto_rotate_toggled(GtkButton *button, gpointer user_data)`
**Triggered by**: [▶ Auto] / [⏸ Auto] button
**Flow**:
1. Toggle `self->auto_rotation_enabled`
2. If enabled:
   - Set button label to [⏸ Auto]
   - Start timer: `g_timeout_add(50, on_auto_rotate_timeout, self)`
3. If disabled:
   - Set button label to [▶ Auto]
   - Stop timer: `g_source_remove()`

#### `on_auto_rotate_timeout(gpointer user_data)`
**Triggered by**: 50ms timer (20× per second)
**Flow**:
1. Increment `self->auto_angle` by 5.0°
2. If angle >= 360.0°:
   - Reset angle to 0.0
   - Increment `self->sample_index`
   - Load next sample image
3. Call `rotate_image()` with new angle
4. Return TRUE to keep timer active

---

## Data Flow Diagram

```
[Image Load]
    ↓
gtk_image_set_from_file() → self->image_widget (display)
g_object_set_data("current-filepath", path) (save path)
    ↓
[Rotation]
    ↓
rotate_image(angle) → pixbuf_to_rgba() → rotate_rgba_nn_padauto()
→ rgba_to_pixbuf() → gtk_image_set_from_pixbuf() (display)
    ↓
[Binarization]
    ↓
binary_load_otsu() → BinaryImage struct (in memory)
binary_save_png() → file (display)
    ↓
[Extraction]
    ↓
binary_find_components() → ComponentList
for each component:
  binary_crop() → BinaryImage
  resize to 28×28
  binary_save_png() → letters/
    ↓
[OCR]
    ↓
for each letter_*.png:
  nn_predict_letter_from_file() → 'A'-'Z'
  append to g_string
gtk_text_buffer_set_text(grid_text_view) (display)
    ↓
[Solve - Placeholder]
    ↓
(Ready for solver.c integration)
```

---

## Key Constants

```c
// Neural Network
#define INPUT_SIZE 784      // 28×28 pixels
#define HIDDEN_SIZE 128     // Middle layer neurons
#define OUTPUT_SIZE 26      // A-Z letters

// Image Processing
#define MIN_COMPONENT_SIZE 5    // Minimum letter size
#define MAX_COMPONENT_SIZE 200  // Maximum letter size
#define LETTER_SIZE 28          // Standardized letter dimension

// Timing
#define AUTO_ROTATE_INTERVAL_MS 50  // 20 FPS
#define AUTO_ROTATE_INCREMENT_DEG 5  // Degrees per frame
```

---

## Error Handling Patterns

### Image Loading
```c
const gchar *path = g_object_get_data(G_OBJECT(image), "current-filepath");
if (!path || *path == '\0') {
  update_status_label(self, "Error: invalid path");
  return;
}
```

### Binary Processing
```c
BinaryImage *bin = binary_load_otsu(path);
if (!bin) {
  update_status_label(self, "Error: failed to binarize");
  return;
}
self->current_binary = bin;
```

### Component Extraction
```c
ComponentList *comps = binary_find_components(self->current_binary);
if (!comps || comps->count == 0) {
  update_status_label(self, "Error: no components found");
  if (comps) binary_free_components(comps);
  return;
}
```

### OCR Processing
```c
char predicted = nn_predict_letter_from_file(filepath);
if (predicted >= 'A' && predicted <= 'Z') {
  g_string_append_c(recognized, predicted);
  letter_count++;
}
```

---

## Memory Management Checklist

✅ **Binary Images**
- Allocate: `binary_load_otsu()` → returns pointer
- Use: Store in `self->current_binary`
- Free: `binary_free()` in finalize or before replacing

✅ **Component Lists**
- Allocate: `binary_find_components()` → returns pointer
- Use: Iterate through boxes, extract components
- Free: `binary_free_components()` after use

✅ **Strings**
- Allocate: `g_string_new()` or `g_strdup()`
- Use: Build grid text or status messages
- Free: `g_string_free()` or `g_free()` in finalize

✅ **Arrays**
- Allocate: `g_ptr_array_new_with_free_func()`
- Use: Store sample paths
- Free: `g_ptr_array_unref()` in finalize

✅ **Timers**
- Start: `g_timeout_add(interval, callback, data)`
- Stop: `g_source_remove(timeout_id)` before cleanup
- Reset: Store timeout_id to reference later

---

## Testing Checklist

- [ ] Load PNG image
- [ ] Load JPEG image
- [ ] Auto-rotate (should show smooth rotation)
- [ ] Manual rotate (set angle, click rotate)
- [ ] Binarize (should show B&W image)
- [ ] Extract (should create letters/ folder)
- [ ] OCR (should display recognized text)
- [ ] Solve (placeholder message)
- [ ] Memory: No segfaults after operations
- [ ] Performance: Each operation completes < 1 second
- [ ] UI responsiveness: Buttons responsive during processing

---

**Quick Reference** - Keep this card handy while coding!
