// TemplateWindows.cpp : Defines the entry point for the game.
//

#include "stdafx.h"
#include <string>
#include "main.h"
#include <wiRenderPath2D_BindLua.h>
#define MAX_LOADSTRING 100




// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
wi::Application game;					// Wicked Engine Application

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    BOOL dpi_success = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    assert(dpi_success);

	wi::arguments::Parse(lpCmdLine); // if you wish to use command line arguments, here is a good place to parse them...

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TEMPLATEWINDOWS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform game initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TEMPLATEWINDOWS));

	// just show some basic info:
	game.infoDisplay.active = true;
	game.infoDisplay.watermark = true;
	game.infoDisplay.resolution = true;
	game.infoDisplay.fpsinfo = true;


	game.Initialize();

	using namespace wi;



	enum State {
		Traveling,
		Coiling,
	};


	class SlinkyScene : public wi::RenderPath3D
	{

		SpriteFont curr_pivot_dbg;
		SpriteFont walk_progress_dbg;
		SpriteFont move_lock_dbg;

		SpriteFont state_dbg;
		SpriteFont debug5;
		SpriteFont debug6;

		ecs::Entity sceneroot;
		ecs::Entity main_camera;
		ecs::Entity camera_pivot;

		float aim_roll = 0.;

		float base_speed = 2.5;
		float min_speed = 0.2;
		float anim_edge_slowdown = 0.8;

		float aim_speed = 10.;
		bool move_allowed = false;
		float edge_threshold = 0.035;


		State state = State::Coiling;

		XMFLOAT2 mouse_pointer = XMFLOAT2(0, 0);



		float curr_progress = 0;
		float curr_anim_speed = 0;

		ecs::Entity current_pivot;
		scene::TransformComponent* curr_pivot_tf;
		scene::TransformComponent* prev_pivot_tf;

		ecs::Entity pivot_one;
		scene::TransformComponent* pivot_one_tf;
		ecs::Entity pivot_two;
		scene::TransformComponent* pivot_two_tf;


	public:
		SlinkyScene() {

			curr_pivot_dbg.params.posX = 2;
			curr_pivot_dbg.params.posY = 120;
			curr_pivot_dbg.params.size = 20;
			state_dbg.params.posX = 100;
			state_dbg.params.posY = 120;
			state_dbg.params.size = 20;
			walk_progress_dbg.params.posX = 2;
			walk_progress_dbg.params.posY = 100;
			walk_progress_dbg.params.size = 20;
			move_lock_dbg.params.posX = 2;
			move_lock_dbg.params.posY = 160;
			move_lock_dbg.params.size = 20;
			debug5.params.posX = 2;
			debug5.params.posY = 200;
			debug5.params.size = 20;
			debug6.params.posX = 2;
			debug6.params.posY = 220;
			debug6.params.size = 20;
			AddFont(&curr_pivot_dbg);
			AddFont(&walk_progress_dbg);

			sceneroot = scene::LoadModel("Resources/Slinky.wiscene", XMMatrixIdentity(), true);



		}

		void Start() override {
			using namespace scene;

			RenderPath3D::Start();
			Scene& scene = GetScene();

			move_lock_dbg.SetText("Just starting, moving disallowed!");


			main_camera = scene.Entity_FindByName("SlinkyCam");
			camera_pivot = scene.Entity_FindByName("CameraPivot");

			CameraComponent* scene_camera = scene.cameras.GetComponent(main_camera);
			camera = scene_camera; // camera variable exists within RenderPath3D object

			pivot_one = scene.Entity_FindByName("ONE");
			pivot_one_tf = scene.transforms.GetComponent(pivot_one);
			pivot_two = scene.Entity_FindByName("TWO");
			pivot_two_tf = scene.transforms.GetComponent(pivot_two);

			current_pivot = pivot_one;
			curr_pivot_tf = pivot_one_tf;
			prev_pivot_tf = pivot_two_tf;


			// We start the slinky walk
			ecs::Entity walk_anim_entity = scene.Entity_FindByName("Walk");
			AnimationComponent* walk_anim = scene.animations.GetComponent(walk_anim_entity);

			if (walk_anim != nullptr) {
				walk_anim->Play();
			}

			mouse_pointer = XMFLOAT2(input::GetPointer().x, input::GetPointer().y);

			curr_pivot_dbg.SetText("Current pivot :1\n");
		}

		void Update(float dt) override {
			using namespace wi::scene;

			RenderPath3D::Update(dt);
			Scene& scene = GetScene();

			ecs::Entity slinky_character = scene.Entity_FindByName("SlinkyCharacter");
			TransformComponent* character_tf = scene.transforms.GetComponent(slinky_character);

			ecs::Entity walk_anim_entity = scene.Entity_FindByName("Walk");
			AnimationComponent* walk_anim = scene.animations.GetComponent(walk_anim_entity);
			bool reverse = false;
			// we slow down at the edges
			if (walk_anim != nullptr) {

				curr_progress = walk_anim->timer / walk_anim->GetLength();
				curr_anim_speed = walk_anim->speed;
				walk_progress_dbg.SetText(std::to_string(curr_progress));
				float close_to_edges = abs(curr_progress - 0.5) * 2;
				close_to_edges = anim_edge_slowdown * close_to_edges;

				if (walk_anim->speed > 0.) {
					walk_anim->speed = (1. - close_to_edges) * anim_edge_slowdown * base_speed;

				}
				else {
					reverse = true;
					walk_anim->speed = -(1. - close_to_edges) * anim_edge_slowdown * base_speed;

				}
			}

			//we change the next jump aim

			if (input::Press((input::BUTTON)'D') || input::Hold((input::BUTTON)'D', 0, true)) {
				aim_roll = aim_speed;
			}
			else if (input::Press((input::BUTTON)'Q') || input::Hold((input::BUTTON)'Q', 0, true)) {
				aim_roll = -aim_speed;
			}
			else {
				aim_roll = 0;
			}



			ecs::Entity next_dir = scene.Entity_FindByName("NextJumpDirectionChooser");
			TransformComponent* next_dir_tf = scene.transforms.GetComponent(next_dir);


			ecs::Entity next_jump_indic = scene.Entity_FindByName("NextJumpIndic");
			TransformComponent* next_jump_indic_tf = scene.transforms.GetComponent(next_jump_indic);
			ecs::Entity opposite_jump_indic = scene.Entity_FindByName("OppositeIndic");
			TransformComponent* opposite_jump_indic_tf = scene.transforms.GetComponent(opposite_jump_indic);
	
			//we align to the next jump aim
			if (next_dir_tf == nullptr) return; 
			next_dir_tf->RotateRollPitchYaw(XMFLOAT3(0, aim_roll * dt, 0));
			




			// game logic


			

			if (curr_pivot_tf == nullptr || prev_pivot_tf == nullptr) return;
			// currently traveling


			HierarchyComponent* current_pivot_hierarchy = scene.hierarchy.GetComponent(current_pivot);
			HierarchyComponent* character_hierarchy = scene.hierarchy.GetComponent(slinky_character);

			if (current_pivot_hierarchy == nullptr || character_hierarchy == nullptr) return;
			if (curr_progress > edge_threshold && curr_progress <= 1-edge_threshold) {
				if (state != State::Traveling) {
					//we switch current pivot if we just entered traveling state
					state = State::Traveling;
					if (curr_anim_speed < 0) {
						current_pivot = pivot_one;
						curr_pivot_tf = pivot_one_tf;
						prev_pivot_tf = pivot_two_tf;
					}
					else {
						current_pivot = pivot_two;
						curr_pivot_tf = pivot_two_tf;
						prev_pivot_tf = pivot_one_tf;
					}
					if (current_pivot == pivot_two) {
						curr_pivot_dbg.SetText("Current pivot: 2\n");
						OutputDebugString(L"Current pivot: 2\n");
					}
					else {
						curr_pivot_dbg.SetText("Current pivot :1\n");
						OutputDebugString(L"Current pivot: 1\n");
					}

					auto rotation = next_dir_tf->GetRotation();
					next_dir_tf->ClearTransform();
					next_dir_tf->Translate(curr_pivot_tf->GetPosition());
					next_dir_tf->Rotate(rotation);
					state_dbg.SetText("Traveling");
				}
			}
			else {
				if (state != State::Coiling) {
					// we allow movement if we're just out of traveled state
					move_allowed = true;
					OutputDebugString(L"Move allowed~\n");
					state_dbg.SetText("Coiling");
					state = State::Coiling;
					OutputDebugString(L"changing state to Coiling\n");
				}
				if (move_allowed) {
					OutputDebugString(L"We're moving here\n");	
					// are we going in reverse ?
					bool reverse = current_pivot == pivot_one;

					auto up = XMFLOAT3(0, 1, 0);

					auto pivot_center_position = next_dir_tf->GetPosition();
					auto next_position= next_jump_indic_tf->GetPosition();

					auto opposite_position=opposite_jump_indic_tf->GetPosition();
					//
					////
					//auto look_matrix =  XMMatrixLookAtRH(XMLoadFloat3(&next_position),
					//		XMLoadFloat3(&pivot_center_position),
					//		XMLoadFloat3(&up));
					//if (reverse) {
					//	look_matrix = XMMatrixLookAtRH(XMLoadFloat3(&next_position),
					//		XMLoadFloat3(&pivot_center_position),
					//		XMLoadFloat3(&up));
					//}
 				//	character_tf->ClearTransform();
					//character_tf->MatrixTransform(look_matrix);
					auto old_pos = character_tf->GetPosition();
					auto vec = XMLoadFloat3(&pivot_center_position) - XMLoadFloat3(&old_pos);
					character_tf->ClearTransform();
					character_tf->Translate(next_position);
					if (reverse) {
						character_tf->Rotate(opposite_jump_indic_tf->GetRotation());
					}
					else {
						character_tf->Rotate(opposite_jump_indic_tf->GetRotationV());
						character_tf->RotateRollPitchYaw(XMFLOAT3(0,math::PI,0));
					}
					
				}
			}

			XMFLOAT2 mouse_delta = XMFLOAT2(input::GetPointer().x - mouse_pointer.x, input::GetPointer().y - mouse_pointer.y);
			TransformComponent* camera_pivot_tf = scene.transforms.GetComponent(camera_pivot);

			//we handle the arrow's rotation
			ecs::Entity arrow = scene.Entity_FindByName("Arrow");

			TransformComponent* arrow_pivot_tf = scene.transforms.GetComponent(arrow);

			if (arrow_pivot_tf != nullptr)   		{
				arrow_pivot_tf->RotateRollPitchYaw(XMFLOAT3(0, dt, 0));
			}

			//we handle the camera at the end because we need the pivots to be set beforehand

			if (camera_pivot_tf != nullptr) {
				//we store its rotation
				XMFLOAT4 rotation = camera_pivot_tf->GetRotation();
				if (state == State::Traveling) {
					//We lerp the camera to the next pivot, slowly
					camera_pivot_tf->ClearTransform();
					camera_pivot_tf->Translate(math::Lerp(camera_pivot_tf->GetPosition(), curr_pivot_tf->GetPosition(), XMFLOAT3( dt, dt, dt)));
					camera_pivot_tf->Rotate(rotation);
				}
				//we rotate the camera according to the mouse
				camera_pivot_tf->RotateRollPitchYaw(XMFLOAT3(0, mouse_delta.x * dt, 0));
			}


			mouse_pointer = XMFLOAT2(input::GetPointer().x, input::GetPointer().y);

		}
	};

	SlinkyScene path;
	game.ActivatePath(&path);
	

	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {

			game.Run(); // run the update - render loop (mandatory)

		}
	}

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TEMPLATEWINDOWS));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TEMPLATEWINDOWS);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);


   game.SetWindow(hWnd); // assign window handle (mandatory)


   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the game menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_SIZE:
    case WM_DPICHANGED:
		if (game.is_window_active)
			game.SetWindow(hWnd);
        break;
	case WM_CHAR:
		switch (wParam)
		{
		case VK_BACK:
			wi::gui::TextInputField::DeleteFromInput();
			break;
		case VK_RETURN:
			break;
		default:
		{
			const wchar_t c = (const wchar_t)wParam;
			wi::gui::TextInputField::AddInput(c);
		}
		break;
		}
		break;
	case WM_INPUT:
		wi::input::rawinput::ParseMessage((void*)lParam);
		break;
	case WM_KILLFOCUS:
		game.is_window_active = false;
		break;
	case WM_SETFOCUS:
		game.is_window_active = true;
		break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
