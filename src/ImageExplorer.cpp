#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <random>
#include <algorithm>
#include <initializer_list>
#include <chrono>

#include <raylib.h>

#define TARGET_FPS 60

#define BDRY_OFFSET 10
#define PADDING 4.0f
#define CORNER_RADIUS 6
#define INFO_MESSAGE_COUNT 15
#define FONT_SPACING 0.0f
#define BUTTON_SIZE 30

#define DIA_SHOW_IMAGE_DURATION (TARGET_FPS * 4)

#define COLOR_HIGHLIGHT_0 Color{ 200, 200, 255, 120 }
#define COLOR_HIGHLIGHT_1 Color{ 150, 150, 255, 180 }
#define COLOR_HIGHLIGHT_2 Color{ 255, 161, 30, 120 }

#define COLOR_PROGRESS_FG COLOR_HIGHLIGHT_2

#define N_BG_COLORS 3
int bg_color_ptr = 0;
const Color bg_colors[N_BG_COLORS]{RAYWHITE, DARKGRAY, BLACK};

int64_t frame_counter = 0;
int screen_width = 800;
int screen_height = 600;
Vector2 mouse_pos = {0};
Vector2 mouse_delta = {0};

enum button_enum {
    Fullscreen,
    To_end,
    Next_media,
    Prev_media,
    To_start,
    Trash,
    RotateCW,
    RotateCCW,
    SwitchBG,
    SIZE
};

struct Info_box;
Info_box* main_info_box = nullptr;

Font font;

Vector2 ul_corner{0};
Vector2 ur_corner{0};
Vector2 dl_corner{0};
Vector2 dr_corner{0};
Vector2 center{0};

enum class media_enum {
    Image, Video
};

struct Media_elem {
    std::string path;
    media_enum type;
    int rotation = 0;
    int64_t creation_time = 0;
};

std::vector<Media_elem> media_elems;
int64_t media_ptr = -1;
Image image;

Color get_anti_bg_color() {
    Color color;
    if(bg_color_ptr + 1 < N_BG_COLORS)
	color = bg_colors[bg_color_ptr + 1];
    else
	color = bg_colors[0];
    return color;
}

struct {

    int64_t operator()(int64_t idx) {
	return idx_array[idx];
    }

    void make_alphabetical() {
	if(state != Alpha || idx_array.size() != media_elems.size()) {
	    if(idx_array.size() != media_elems.size()) {
		idx_array.clear();
		idx_array.reserve(media_elems.size());
		for(int i = 0; i < media_elems.size(); ++i)
		    idx_array.push_back(i);
	    }
	    else {
		for(int i = 0; i < idx_array.size(); ++i)
		    idx_array[i] = i;  
	    }
	    state = Alpha;
	}
    }

    void make_chronological_up() {
	if(state != Chrono_up){
	    make_alphabetical();
	    if(!idx_array.empty()){
		std::sort(idx_array.begin(), idx_array.end(), [&](int64_t a, int64_t b){
		    return media_elems[a].creation_time > media_elems[b].creation_time;
		});
	    }
	    state = Chrono_up;
	}
    }

	
    void make_chronological_down() {
	if(state != Chrono_down) {
	    make_alphabetical();
	    if(!idx_array.empty()){
		std::sort(idx_array.begin(), idx_array.end(), [&](int64_t a, int64_t b){
		    return media_elems[a].creation_time < media_elems[b].creation_time;
		});
	    }
	    state = Chrono_down;
	}
    }

    void make_random() {
	if(state != Random){
	    make_alphabetical();
	    static auto rng = std::default_random_engine{};
	    if(!idx_array.empty())
		std::shuffle(idx_array.begin(), idx_array.end(), rng);
	    state = Random;
	}
    }

private:
    
    int state = -1;
    enum state_enum {Alpha, Chrono_up, Chrono_down, Random};
    std::vector<int64_t> idx_array;
} midx;

struct Button {
    
    const Vector2* anker; // provide only global variables
    Rectangle button; // offset from anker and button size
    Texture2D texture;
    enum {idle = 0, hovering = 1} button_state;

    Button(const char* path, Rectangle button, Vector2* anker)
	: texture(LoadTexture(path)), button(button), anker(anker) {}
    ~Button() {	UnloadTexture(texture); }
    
    void draw(){

	Rectangle button_abs{anker->x + button.x, anker->y + button.y, button.width, button.height};

	if(CheckCollisionPointRec(mouse_pos, button_abs))
	    button_state = hovering;
	else
	    button_state = idle;

	if(mouse_delta.x * mouse_delta.y > 0.0001 || button_state == hovering)
	    last_visib_request_frame = frame_counter;
	if(last_visib_request_frame + visibility_duration >= frame_counter) {
	    float roundness = float(CORNER_RADIUS) / (std::min(button_abs.width, button_abs.height) * 0.5f);
	    DrawRectangleRounded(button_abs, roundness, 3, button_state == idle ? COLOR_HIGHLIGHT_0 : COLOR_HIGHLIGHT_1);
	    DrawTexturePro(texture,
			   Rectangle{0, 200 * (float)button_state, 200, 200},
			   button_abs, Vector2{0}, 0, WHITE);
	}
    }

    bool is_pressed() {
	Rectangle button_abs{anker->x + button.x, anker->y + button.y, button.width, button.height};
        if(CheckCollisionPointRec(mouse_pos, button_abs) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
	    return true;
	return false;
    }

private:
    static const int visibility_duration = TARGET_FPS * 2;
    int64_t last_visib_request_frame = 0;
    
};

struct Info_box {

    Info_box(Vector2 offset, Vector2* anker, int font_size, float font_padding)
	: offset(offset), anker(anker), font_size(font_size), font_padding(font_padding) {}

    Vector2 offset;
    Vector2* anker;
    int font_size;
    float font_padding;
    
    void log(std::string s, Color color = BLACK) {
	if(msgs.ptr + 1 < INFO_MESSAGE_COUNT) {
	    ++msgs.ptr;
	    msgs.data[msgs.ptr] = msg{s, color};
	    if(msgs.capacity < INFO_MESSAGE_COUNT)
		++msgs.capacity;
	}
	else {
	    msgs.ptr = 0;
	    msgs.data[msgs.ptr] = msg{s, color};
	}
	last_visib_request_frame = frame_counter;
    }

    void log_error(std::string s) { log(s, MAROON); }
    void log_warning(std::string s) { log(s, ORANGE); }

    void draw() {
	if(last_visib_request_frame + visibility_duration >= frame_counter && msgs.capacity > 0){
	    Vector2 box_dl{anker->x + offset.x, anker->y + offset.y}; // dl -> down left
	    max_string_size = 0;
	    for (int i = 0; i < msgs.capacity; ++i) {
		Vector2 string_size = MeasureTextEx(font, msgs.data[i].s.c_str(), font_size, font_padding);
		if(string_size.x > max_string_size)
		    max_string_size = string_size.x;
	    }

	    float height = msgs.capacity * (float(font_size) + PADDING) + PADDING;
	    float width = max_string_size + PADDING * 2;
	    float roundness = float(CORNER_RADIUS) / (std::min(width, height) * 0.5f);
	    Color box_color = get_anti_bg_color();
	    box_color.a = 120;
	    box_color.r = std::max(box_color.r, (unsigned char)40);
	    box_color.g = std::max(box_color.g, (unsigned char)40);
	    box_color.b = std::max(box_color.b, (unsigned char)40);
	    DrawRectangleRounded(Rectangle{
		    box_dl.x - PADDING, box_dl.y - height, width, height},
		roundness, 3, box_color);
	    
	    for (int i = msgs.ptr; i >= 0; --i) {
		Vector2 pos{box_dl.x, box_dl.y - float(((msgs.ptr - i) + 1) * (font_size + PADDING))};
		DrawTextEx(font, msgs.data[i].s.c_str(), pos, font_size, FONT_SPACING, msgs.data[i].color);
	    }

	    for (int i = msgs.capacity - 1; i > msgs.ptr; --i) {
		Vector2 pos{box_dl.x, box_dl.y - float((msgs.capacity - (i - msgs.ptr) + 1) * (font_size + PADDING))};
		DrawTextEx(font, msgs.data[i].s.c_str(), pos, font_size, FONT_SPACING, msgs.data[i].color);
	    }
	}
    }

private:

    struct msg {
	std::string s;
	Color color;
    };
    
    struct {
	msg data[INFO_MESSAGE_COUNT];
	int ptr = -1;
	int capacity = 0;
    } msgs;

    float max_string_size = 0;

    static const int visibility_duration = TARGET_FPS * 4;
    int64_t last_visib_request_frame = 0;
};

enum Menue_style : uint32_t {
    MENUE_RIGHT_BOUND         = 1,      // default is left bound.
    MENUE_EXCLUSIVE_SELECTION = 1 << 1, // default is independent selection.
};

struct Menue_item {
    const char* msg;
    int font_size;
    Color color;
    bool active = false;
};

// NOTE: the icon texture should be a 1x5 texture, where 1-4 are
// the states of the drop down icon and 5 the selector icon.
struct Drop_down_menue {

    Drop_down_menue(std::initializer_list<Menue_item> menue_items, uint32_t style, const char* icon_path,
		    Vector2* anker, Vector2 offset, Color color_idle, Color color_hover)
	: menue_items{menue_items}, style(style), font_size(font_size), icon_texture(LoadTexture(icon_path)),
	  anker(anker), offset(offset), color_idle(color_idle), color_hover(color_hover) {}

    std::vector<Menue_item> menue_items;
    uint32_t style;
    int font_size;
    Texture2D icon_texture;
    Vector2* anker;
    Vector2 offset;
    Color color_idle;
    Color color_hover;
    int active_item = 1; // first item of the list must be the default with exclusive selection.

    void draw_and_eval() {
	
	if(mouse_delta.x * mouse_delta.y)
	    last_visib_request_frame = frame_counter;
	if(last_visib_request_frame + visibility_duration < frame_counter)
	    return;
	
	float accum_height = 0;
	for(int i = 0; i < menue_items.size(); ++i) {

	    float icon_size = menue_items[i].font_size + PADDING;
	    bool is_idle = true;
	    Vector2 text_size = MeasureTextEx(font, menue_items[i].msg, menue_items[i].font_size, FONT_SPACING);
	    float width = text_size.x + icon_size + PADDING * 2;
	    float x = anker->x + offset.x + (style & MENUE_RIGHT_BOUND ? -width : icon_size);
	    float y = accum_height + anker->y + offset.y;
	    float height = menue_items[i].font_size + PADDING * 2;
	    
	    if(CheckCollisionPointRec(mouse_pos, Rectangle{x, y, width, height})) {
		last_visib_request_frame = frame_counter;
		is_idle = false;
		if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		    if((style & MENUE_EXCLUSIVE_SELECTION) && i != 0)
			active_item = i;
		    else
			menue_items[i].active = !menue_items[i].active;
		}
		accum_height += 3;
	    }

	    float roundness = float(CORNER_RADIUS) / float(std::min(width, menue_items[i].font_size + PADDING) * 0.5f);
	    DrawRectangleRounded(Rectangle{
		    x - PADDING, y - PADDING, width, menue_items[i].font_size + PADDING},
		roundness, 3, is_idle ? color_idle : color_hover);
	    DrawTextEx(font, menue_items[i].msg, Vector2{x, y}, menue_items[i].font_size, FONT_SPACING, menue_items[i].color);

	    if(i == 0) {
		Rectangle icon {
		    x + (style & MENUE_RIGHT_BOUND ? width - (icon_size + PADDING) : 0),
		    y,
		    float(menue_items[i].font_size),
		    float(menue_items[i].font_size),
		};
		DrawTexturePro(icon_texture,
			       Rectangle{0, 200 * ((float)(!is_idle) + 2 * (!menue_items[i].active)), 200, 200},
			       icon, Vector2{0}, 0, Color{0,0,0,255});
		if(!menue_items[i].active)
		    break;
	    }
	    else if(menue_items[i].active) {
		Rectangle icon {
		    x + (style & MENUE_RIGHT_BOUND ? width - (icon_size + PADDING) : 0),
		    y,
		    float(menue_items[i].font_size),
		    float(menue_items[i].font_size),
		};
		DrawTexturePro(icon_texture, Rectangle{0, 200 * 4, 200, 200},
			       icon, Vector2{0}, 0, Color{0,0,0,255});
	    }
	
	    accum_height += height;
	    
	    if((style & MENUE_EXCLUSIVE_SELECTION) && i != 0)
		menue_items[i].active = false;
	}
	
	if(style & MENUE_EXCLUSIVE_SELECTION)
	    menue_items[active_item].active = true;
    }
    
private:
    
    static const int visibility_duration = TARGET_FPS * 2;
    int64_t last_visib_request_frame = 0;
};

struct Progress_bar {

    Progress_bar(Vector2* left_anker, Vector2 offset, float* right_bound, float height,
		 Color fg_color)
	: left_anker(left_anker), offset(offset), right_bound(right_bound), height(height),
	  fg_color(fg_color) {}
    
    Vector2* left_anker;
    Vector2 offset;
    float* right_bound;
    float height;
    Color fg_color;
    
    void draw_and_eval(auto& vector, int64_t* idx) {

	if(mouse_delta.x * mouse_delta.y)
	    last_visib_request_frame = frame_counter;
	if(last_visib_request_frame + visibility_duration < frame_counter)
	    return;
	
	float x = left_anker->x + offset.x;
	float y = left_anker->y + offset.y;
	float width = std::max(0.0f, ((*right_bound) - BDRY_OFFSET) - x);
	Color bg_color = get_anti_bg_color();
	bg_color.a = 120;

	DrawRectangleRounded(Rectangle{ x, y, width, height}, 1.0f, 3, bg_color);
	if(!(vector.empty()) && (*idx) >= 0) {

	    int font_size = int(height * 0.75f);
	    const char* idx_text = TextFormat("%d/%d", (*idx) + 1, vector.size());
	    Vector2 text_size = MeasureTextEx(font, idx_text, font_size, FONT_SPACING);
	    
	    float progress = std::min(width, float(float((*idx) + 1) / float(vector.size())) * (width - (text_size.x + PADDING * 2)))
		+ (text_size.x + PADDING * 2);
	    DrawRectangleRounded(Rectangle{ x, y, progress, height}, 1.0f, 3, fg_color);
	    Vector2 text_pos{x + (progress - (text_size.x + PADDING)), y + (height - text_size.y) / 2};
	    DrawTextEx(font, idx_text, text_pos, font_size, FONT_SPACING, BLACK);

	    float col_rec_x = x + text_size.x / 2 + PADDING;
	    Rectangle col_rec{col_rec_x, y, (x + width) - col_rec_x, height};
	    if(CheckCollisionPointRec(mouse_pos, col_rec) && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
		*idx = int64_t(std::round(((mouse_pos.x - col_rec.x) / col_rec.width) * (vector.size() - 1)));
	}
	
    }

private:
    
    static const int visibility_duration = TARGET_FPS * 2;
    int64_t last_visib_request_frame = 0;
};

void toggle_fullscreen_window(){

    static int previous_width;
    static int previous_height;
    
    if(!IsWindowFullscreen()) {
	int monitor = GetCurrentMonitor();
	SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
	ToggleFullscreen();
    }
    else {
	ToggleFullscreen();
	SetWindowSize(previous_width, previous_height);
    }

    previous_width = screen_width;
    previous_height = screen_height;
    
}

int64_t get_creation_time(const std::filesystem::path& path) {

    namespace fs = std::filesystem;
    std::chrono::time_point tp = fs::last_write_time(path);
    auto duration = tp.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

void load_media(std::string source_path) {

    media_ptr = -1;
    
    namespace fs = std::filesystem;
    try {
	const fs::path path(source_path);
	if (!fs::exists(path)){
	    main_info_box->log_error("Couldn't load the path.");
	    return;
	}
    
	int64_t cant_read_counter = 0;
	int64_t counter = 0, image_counter = 0, video_counter = 0;
	std::string path_extension;
	auto load_file = [&](const fs::path& path){
	    path_extension = path.extension().string();
	    std::transform(path_extension.begin(), path_extension.end(), path_extension.begin(),
			   [](unsigned char c) { return std::tolower(c); });
	    if (path_extension == ".jpg" || path_extension == ".png" || path_extension == ".bmp"
		|| path_extension == ".tga" || path_extension == ".svg" || path_extension == ".gif") {
		media_elems.push_back({path.string(), media_enum::Image, 0, get_creation_time(path)});
		++image_counter;
	    }
	    else if (path_extension == ".mp4" || path_extension == ".mov") {
		media_elems.push_back({path.string(), media_enum::Video, 0, get_creation_time(path)});
		++video_counter;
	    }
	    else {
		++cant_read_counter;
		--counter;
	    }
	    ++counter;
	};

	if(fs::is_directory(path)) {
	    for (const auto& entry : fs::recursive_directory_iterator(path)) {
		try {
		    load_file(entry.path());
		}
		catch(fs::filesystem_error const& error){
		    std::cout << "filesystem error: " << error.what() << '\n';
		    ++cant_read_counter;
		}
	    }
	}
	else {
	    load_file(path);
	}
    
	main_info_box->log(TextFormat("Read %d files: Images %d, Videos %d", counter, image_counter, video_counter));
	if(cant_read_counter)
	    main_info_box->log_warning(TextFormat("Failed to read %d files", cant_read_counter));
    }
    catch(fs::filesystem_error const& error){
	main_info_box->log_error(TextFormat("Failed to read directory"));
    }

}

bool handle_dropped_files() {
    if(IsFileDropped()) {
	FilePathList path_list = LoadDroppedFiles();
	for(int i = 0; i < path_list.count; ++i)
	    load_media(std::string(path_list.paths[i]));
	UnloadDroppedFiles(path_list);
	bool result = !media_elems.empty();
	return result;
    }
    return false;
}

Texture2D get_image(int idx) {
    UnloadImage(image);
    image = LoadImage(media_elems[midx(idx)].path.c_str());
    if(media_elems[midx(media_ptr)].rotation)
	ImageRotate(&image, media_elems[midx(media_ptr)].rotation);
    return LoadTextureFromImage(image);
}

Texture2D get_media() {
    if(media_ptr >= 0){
	if(media_elems[midx(media_ptr)].type == media_enum::Image) {
	    return get_image(media_ptr);	    
	}
	else if (media_elems[midx(media_ptr)].type == media_enum::Video) {
	    main_info_box->log_error("Video display not implemented yet.");
	}
    }
    return Texture2D{0};
}

Texture2D get_next_media() {
    if(media_ptr >= 0) {
	if(media_ptr + 1 < media_elems.size())
	    ++media_ptr;
	else
	    main_info_box->log("No new images to show.");
	return get_media();
    }
    return Texture2D{0};
}

Texture2D get_prev_media() {
    if(media_ptr >= 0) {
	if(media_ptr - 1 >= 0)
	    --media_ptr;
	else
	    main_info_box->log("No new images to show.");
	return get_media();
    }
    return Texture2D{0};
}

void draw_default_message(Vector2 center, int font_size, float spacing) {
    
    const int n_msgs = 7;

    struct msg {
	const char* m;
	int font_size;
	Vector2 text_size;
    };

    msg msgs[n_msgs] = {
	msg{"Drop media folders here!", int(float(font_size) * 1.5f)},
	msg{"UP ARROW: Rotate Element CW", font_size},
	msg{"DOWN ARROW: Rotate Element CCW", font_size},
	msg{"RIGHT ARROW: Next Element", font_size},
	msg{"LEFT ARROW: Previous Element", font_size},
	msg{"SPACE: Start Diashow", font_size},
        msg{"You can drag the slider!", font_size}
    };

    float max_width = 0;
    float total_height = 0;
    for(int i = 0; i < n_msgs; ++i) {
	msgs[i].text_size = MeasureTextEx(font, msgs[i].m, msgs[i].font_size, spacing);
	max_width = msgs[i].text_size.x > max_width ? msgs[i].text_size.x : max_width;
	total_height += msgs[i].text_size.y; 
    }

    Color color = get_anti_bg_color();
    color.a = 150;

    float accum_text_height = 0;
    for(int i = 0; i < n_msgs; ++i) {
	DrawTextEx(font, msgs[i].m,
		   Vector2{center.x - max_width * 0.5f, center.y + (accum_text_height - total_height * 0.5f)},
		   msgs[i].font_size, FONT_SPACING, color);
	accum_text_height += msgs[i].text_size.y;
    }
}

int main() {

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screen_width, screen_height, "ImageExplorer");
    SetTargetFPS(TARGET_FPS);               // Set desired framerate (frames-per-second)
    int framesCounter = 0;          // Useful to count frames
    font = LoadFontEx("resources/DejaVuSansMono.ttf", 80, 0, 0); //GetFontDefault();
    //SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    
    // UI elements
    Button buttons[SIZE] = {
	{"resources/Fullscreen.png", Rectangle{-(BUTTON_SIZE+BDRY_OFFSET), -(BUTTON_SIZE+BDRY_OFFSET), BUTTON_SIZE, BUTTON_SIZE}, &dr_corner},
	{"resources/ToEnd.png", Rectangle{-(BUTTON_SIZE+BDRY_OFFSET)*2, -(BUTTON_SIZE+BDRY_OFFSET),BUTTON_SIZE, BUTTON_SIZE}, &dr_corner},
	{"resources/Rightarrow.png", Rectangle{-(BUTTON_SIZE+BDRY_OFFSET)*3, -(BUTTON_SIZE+BDRY_OFFSET),BUTTON_SIZE, BUTTON_SIZE}, &dr_corner},
	{"resources/Leftarrow.png", Rectangle{-(BUTTON_SIZE+BDRY_OFFSET)*4, -(BUTTON_SIZE+BDRY_OFFSET), BUTTON_SIZE, BUTTON_SIZE}, &dr_corner},
	{"resources/ToStart.png", Rectangle{-(BUTTON_SIZE+BDRY_OFFSET)*5, -(BUTTON_SIZE+BDRY_OFFSET),BUTTON_SIZE, BUTTON_SIZE}, &dr_corner},
	{"resources/Trash.png", Rectangle{(BDRY_OFFSET), -(BUTTON_SIZE+BDRY_OFFSET), BUTTON_SIZE, BUTTON_SIZE}, &dl_corner},
	{"resources/RotateCW.png", Rectangle{(BUTTON_SIZE+BDRY_OFFSET) + BDRY_OFFSET, -(BUTTON_SIZE+BDRY_OFFSET), BUTTON_SIZE, BUTTON_SIZE}, &dl_corner},
	{"resources/RotateCCW.png", Rectangle{(BUTTON_SIZE+BDRY_OFFSET)*2 + BDRY_OFFSET, -(BUTTON_SIZE+BDRY_OFFSET), BUTTON_SIZE, BUTTON_SIZE}, &dl_corner},
	{"resources/SwitchBG.png", Rectangle{(BUTTON_SIZE+BDRY_OFFSET)*3 + BDRY_OFFSET, -(BUTTON_SIZE+BDRY_OFFSET), BUTTON_SIZE, BUTTON_SIZE}, &dl_corner},
    };

    Drop_down_menue mode_select {
	{
	    Menue_item{"Media Order", 20, BLACK},
	    Menue_item{"Alphabetical", 18, BLACK},
	    Menue_item{"Chronological", 18, BLACK},
	    Menue_item{"Anti Chronological", 18, BLACK},
	    Menue_item{"Random", 18, BLACK}
	},
	MENUE_RIGHT_BOUND | MENUE_EXCLUSIVE_SELECTION,
	"resources/Dropdown.png",
	&ur_corner, Vector2{-BDRY_OFFSET, BDRY_OFFSET}, COLOR_HIGHLIGHT_0, COLOR_HIGHLIGHT_1
    };

    Texture2D image_tex{0};

    Info_box info_box{Vector2{BDRY_OFFSET, -(BDRY_OFFSET * 2 + BUTTON_SIZE)}, &dl_corner, 18, FONT_SPACING};
    main_info_box = &info_box;

    float right_bound = dr_corner.x + buttons[To_start].button.x;
    Progress_bar progress_bar{&dl_corner,
	Vector2{buttons[SwitchBG].button.x + BUTTON_SIZE + BDRY_OFFSET, -(BUTTON_SIZE + BDRY_OFFSET)},
	&right_bound, BUTTON_SIZE, COLOR_PROGRESS_FG};
    
    int64_t last_cursor_mvnt = 0;
    int64_t cursor_show_duration = TARGET_FPS * 3;

    int64_t prev_media_ptr = media_ptr;
    bool dia_show = false;
    int64_t dia_show_start = 0;

    while (!WindowShouldClose()) {

	++frame_counter;

	if(frame_counter == TARGET_FPS * 2)
	    main_info_box->log("Hi :O", GREEN);
	
	screen_width = GetScreenWidth();
	screen_height = GetScreenHeight();
	mouse_pos = GetMousePosition();
	mouse_delta = GetMouseDelta();

	// update anker points
	ul_corner = {0, 0};
	ur_corner = {(float)screen_width, 0};
	dl_corner = {0, (float)screen_height};
	dr_corner = {(float)screen_width, (float)screen_height};
	center = {(float)screen_width * 0.5f, (float)screen_height * 0.5f};
	right_bound = dr_corner.x + buttons[To_start].button.x;
	
	if(buttons[Fullscreen].is_pressed()) {
	    toggle_fullscreen_window();
	}

	if(buttons[Next_media].is_pressed() || IsKeyPressed(KEY_RIGHT)) {
	    UnloadTexture(image_tex);
	    image_tex = get_next_media();
	    dia_show = false;
	}
	
	if(buttons[Prev_media].is_pressed() || IsKeyPressed(KEY_LEFT)) {
	    UnloadTexture(image_tex);
	    image_tex = get_prev_media();
	    dia_show = false;
	}

	if(buttons[SwitchBG].is_pressed()) {
	    if(bg_color_ptr + 1 < N_BG_COLORS)
		++bg_color_ptr;
	    else
		bg_color_ptr = 0;
	}
	
	if(buttons[Trash].is_pressed()) {
	    media_elems.clear();
	    media_ptr = -1;
	    UnloadTexture(image_tex);
	    image_tex = get_media();
	}

	if(buttons[RotateCW].is_pressed() || IsKeyPressed(KEY_UP)) {
	    if(media_ptr >= 0) {
		media_elems[midx(media_ptr)].rotation += 90;
		ImageRotate(&image, 90);
		image_tex = LoadTextureFromImage(image);
	    }
	}

	if(buttons[RotateCCW].is_pressed() || IsKeyPressed(KEY_DOWN)) {
	    if(media_ptr >= 0) {
		media_elems[midx(media_ptr)].rotation -= 90;
		ImageRotate(&image, -90);
		image_tex = LoadTextureFromImage(image);
	    }
	}

	if(buttons[To_start].is_pressed()) {
	    if(media_ptr >= 0)
		media_ptr = 0;
	}

	if(buttons[To_end].is_pressed()) {
	    if(media_ptr >= 0)
		media_ptr = media_elems.size() - 1;
	}
	
	if(IsKeyPressed(KEY_SPACE)) {
	    dia_show = true;
	    dia_show_start = frame_counter;
	}

	if(dia_show && !((frame_counter - dia_show_start) % DIA_SHOW_IMAGE_DURATION)) {
	    if(media_ptr + 1 >= media_elems.size()) {
		main_info_box->log("End of Diashow.");
		dia_show = false;
	    }
	    else {
		UnloadTexture(image_tex);
		image_tex = get_next_media();
	    }
	}
	
	// get media files
	if(handle_dropped_files()) {
	    media_ptr = 0;
	    UnloadTexture(image_tex);
	    midx.make_alphabetical();
	    image_tex = get_media();
	}

	if(media_ptr >= 0 && prev_media_ptr != midx(media_ptr)) {
	    UnloadTexture(image_tex);
	    image_tex = get_media();
	    prev_media_ptr = midx(media_ptr);
	}

	switch(mode_select.active_item) {
	case 1:
	    midx.make_alphabetical();
	    break;
	case 2:
	    midx.make_chronological_up();
	    break;
	case 3:
	    midx.make_chronological_down();
	    break;
	case 4:
	    midx.make_random();
	}

	// Hide the cursor, if in the way
	if(mouse_delta.x * mouse_delta.y)
	    last_cursor_mvnt = frame_counter;
	if(IsCursorOnScreen() && last_cursor_mvnt + cursor_show_duration <= frame_counter)
	    HideCursor();
	else if (IsCursorHidden())
	    ShowCursor();
	    
	BeginDrawing();
	{	
	    ClearBackground(bg_colors[bg_color_ptr]);

	    // Default message
	    if(media_ptr == -1)
		draw_default_message(center, 20, FONT_SPACING);

	    // Image canvas
	    if(image_tex.width * image_tex.height){

		Rectangle dest;
		float ratio_x = float(screen_width) / float(image_tex.width);
		float ratio_y = float(screen_height) / float(image_tex.height);
		if(ratio_x < ratio_y) {
		    dest.width = screen_width;
		    dest.height = float(image_tex.height) * ratio_x;
		    dest.x = 0;
		    dest.y = (float(screen_height) - dest.height) * 0.5;
		}
		else {
		    dest.width = float(image_tex.width) * ratio_y;
		    dest.height = screen_height;
		    dest.x = (float(screen_width) - dest.width) * 0.5;
		    dest.y = 0;
		}
		
		DrawTexturePro(image_tex,
			       Rectangle{0, 0, float(image_tex.width), float(image_tex.height)},
			       dest, Vector2{0}, 0, WHITE);
	    }

	    // Progress Bar
	    progress_bar.draw_and_eval(media_elems, &media_ptr);
	    
	    // Menues
	    mode_select.draw_and_eval();

	    // Buttons
	    for(int i = 0; i < button_enum::SIZE; ++i){
		buttons[i].draw();
	    }

	    // Info box (messages)
	    info_box.draw();
	}
        EndDrawing();
    }
    CloseWindow();
}

