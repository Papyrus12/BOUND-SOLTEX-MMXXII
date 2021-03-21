#include "stdafx.h"
#include "GuiUtils.hpp"
#include "Application.hpp"

BasicNuklearGui::~BasicNuklearGui()
{
	ShutdownNuklear();
}

void BasicNuklearGui::UpdateNuklearInput(SDL_Event evt)
{
	if (!m_isOpen)
		return;
	m_eventQueue.push(evt);
}

void BasicNuklearGui::ShutdownNuklear()
{
    if (!m_nuklearRunning)
        return;

    g_gameWindow->OnAnyEvent.RemoveAll(this);
    nk_sdl_shutdown();

    m_nuklearRunning = false;
}

bool BasicNuklearGui::Init()
{
	if (m_backgroundFrame)
	{
		m_fromTexture = TextureRes::CreateFromFrameBuffer(g_gl, g_resolution);
		m_bgMesh = MeshGenerators::Quad(g_gl, Vector2(0, g_resolution.y), Vector2(g_resolution.x, -g_resolution.y));
	}
    InitNuklearIfNeeded();
    return true;
}

void BasicNuklearGui::InitNuklearIfNeeded()
{
    if (m_nuklearRunning) {
        return;
	}
	m_nctx = nk_sdl_init((SDL_Window*)g_gameWindow->Handle());

	g_gameWindow->OnAnyEvent.Add(this, &BasicNuklearGui::UpdateNuklearInput);
	{
		struct nk_font_atlas *atlas;
		nk_sdl_font_stash_begin(&atlas);
		struct nk_font *fallback = nk_font_atlas_add_from_file(atlas, Path::Normalize( Path::Absolute("fonts/settings/NotoSans-Regular.ttf")).c_str(), 24, 0);

		// struct nk_font_config cfg_kr = nk_font_config(24);
		// cfg_kr.merge_mode = nk_true;
		// cfg_kr.range = nk_font_korean_glyph_ranges();

		// NK_STORAGE const nk_rune jp_ranges[] = {
		// 	0x0020, 0x00FF,
		// 	0x3000, 0x303f,
		// 	0x3040, 0x309f,
		// 	0x30a0, 0x30ff,
		// 	0x4e00, 0x9faf,
		// 	0xff00, 0xffef,
		// 	0
		// };
		// struct nk_font_config cfg_jp = nk_font_config(24);
		// cfg_jp.merge_mode = nk_true;
		// cfg_jp.range = jp_ranges;

		NK_STORAGE const nk_rune cjk_ranges[] = {
			0x0020, 0x00FF,
			0x3000, 0x30FF,
			0x3131, 0x3163,
			0xAC00, 0xD79D,
			0x31F0, 0x31FF,
			0xFF00, 0xFFEF,
			0x4e00, 0x9FAF,
			0
		};

		struct nk_font_config cfg_cjk = nk_font_config(24);
		cfg_cjk.merge_mode = nk_true;
		cfg_cjk.range = cjk_ranges;

		int maxSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
		Logf("System max texture size: %d", Logger::Severity::Info, maxSize);
		if (maxSize >= FULL_FONT_TEXTURE_HEIGHT && !g_gameConfig.GetBool(GameConfigKeys::LimitSettingsFont))
		{
			nk_font_atlas_add_from_file(atlas, Path::Normalize(Path::Absolute("fonts/settings/DroidSansFallback.ttf")).c_str(), 24, &cfg_cjk);
		}
		
		nk_sdl_font_stash_end();
		nk_font_atlas_cleanup(atlas);
		//nk_style_load_all_cursors(m_nctx, atlas->cursors);
		nk_style_set_font(m_nctx, &fallback->handle);
	}
	
	m_nctx->style.text.color = nk_rgb(255, 255, 255);
	m_nctx->style.button.border_color = nk_rgb(0, 128, 255);
	m_nctx->style.button.padding = nk_vec2(5,5);
	m_nctx->style.button.rounding = 0;
	m_nctx->style.window.fixed_background = nk_style_item_color(nk_rgb(40, 40, 40));
	m_nctx->style.slider.bar_normal = nk_rgb(20, 20, 20);
	m_nctx->style.slider.bar_hover = nk_rgb(20, 20, 20);
	m_nctx->style.slider.bar_active = nk_rgb(20, 20, 20);

    m_nuklearRunning = true;
}

void BasicNuklearGui::Tick(float deltatime)
{
	nk_input_begin(m_nctx);
	while (!m_eventQueue.empty())
	{
		nk_sdl_handle_event(&m_eventQueue.front());
		m_eventQueue.pop();
	}
	nk_input_end(m_nctx);
}

void BasicNuklearGui::NKRender()
{
	nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
}

void BasicNuklearGui::Render(float deltatime)
{
	if (m_backgroundFrame)
	{
		auto rq = g_application->GetRenderQueueBase();
		Transform t;
		MaterialParameterSet params;
		params.SetParameter("mainTex", m_fromTexture);
		params.SetParameter("color", Vector4(1.0f));
		rq->Draw(t, m_bgMesh, g_application->GetGuiTexMaterial(), params);
	}
	
	g_application->ForceRender();
	NKRender();
}


void BasicWindow::Tick(float deltatime)
{
	BasicNuklearGui::Tick(deltatime);

	if (!m_isOpen) {
		Close();
	}
}

bool nk_edit_isfocused(struct nk_context *ctx)
{
	struct nk_window *win;
	if (!ctx || !ctx->current) return false;

	win = ctx->current;
	return win->edit.active;
}

void BasicWindow::EnableInputForEdit(int widgetWidth, int widgetHeight)
{
	bool isFocused = nk_edit_isfocused(m_nctx);
	if (!m_inEdit && isFocused)
	{
		SDL_StartTextInput();

		// XXX(itszn) idk if this works for ime input yet
		SDL_Rect boxrect;

		struct nk_vec2 textBoxPos = nk_widget_position(m_nctx);
		boxrect.x = textBoxPos.x;
		boxrect.y = textBoxPos.y;
		boxrect.w = widgetWidth;
		boxrect.h = widgetHeight;

		SDL_SetTextInputRect(&boxrect);
	}

	m_isFocused |= isFocused;
}

void BasicWindow::Render(float deltatime)
{
	m_isFocused = false;

	if (m_isOpen)
	{
		if (nk_begin(
			m_nctx,
			*m_name,
			nk_rect(
				g_resolution.x / 2 - m_width / 2,
				g_resolution.y / 2 - m_height / 2,
				m_width, m_height),
			m_windowFlag))
		{
			// Window contents
			DrawWindow();
		}
		else
		{
			m_isOpen = false;
		}
		nk_end(m_nctx);
	}

	if (m_inEdit && !m_isFocused)
	{
		SDL_StopTextInput();
	}

	m_inEdit = m_isFocused;

	BasicNuklearGui::Render(deltatime);
}

void BasicWindow::Close()
{
	m_isOpen = false;
	OnClose();

	if (m_inEdit)
	{
		SDL_StopTextInput();
		m_inEdit = false;
	}

	g_application->RemoveTickable(this);
}

bool BasicWindow::OnKeyPressedConsume(SDL_Scancode code)
{
	if (m_inEdit)
		return true;

	return BasicNuklearGui::OnKeyPressedConsume(code);
}

bool BasicPrompt::Init()
{
	if (!BasicWindow::Init())
		return false;
	return true;
}

bool BasicPrompt::OnKeyPressedConsume(SDL_Scancode code)
{
	// XXX this actually doesn't trigger while doing SDL input :think:
	if (code == SDL_SCANCODE_ESCAPE && m_isOpen)
	{
		Close();
		return true;
	}

	if (code == SDL_SCANCODE_RETURN && m_isOpen)
	{
		m_submitted = true;
		Close();
		return true;
	}
	return BasicWindow::OnKeyPressedConsume(code);
}

void BasicPrompt::OnClose()
{
	OnResult.Call(m_submitted, m_submitted ? m_data : nullptr);
}

void nk_multiline_label(struct nk_context* ctx, const char* allText, nk_text_alignment ali, float width)
{
	const struct nk_user_font *font = ctx->style.font;

	const char* text = allText;
	const char* end = NULL;
	do
	{
		// Also split on new lines
		const char* next = strchr(text, '\n');

		end = next;
		if (end == NULL)
			end = text + strlen(text);

		// Try to wrap text if needed
		int textEnd = 0;
		int maxlen = 0;
		do
		{
			maxlen = end - text;
			textEnd = maxlen;

			// We are going to try to find the max length that fits
			while (
				font->width(font->userdata, font->height, text, textEnd) > width
				&& textEnd > 1)
			{
				// Decrease the size until it fits
				textEnd--;
			}

			// Draw that part of the text
			nk_text(ctx, text, textEnd, ali);

			// Truncate to rest of string
			text += textEnd;
		} while (textEnd < maxlen);

		text = next;
		if (text)
			text++;
	} while (text != NULL);
}

void BasicPrompt::DrawWindow()
{
	struct nk_vec2 start_pos = nk_widget_position(m_nctx);

	nk_layout_set_min_row_height(m_nctx, 20);
	nk_layout_row_dynamic(m_nctx, 20, 1);

	nk_multiline_label(m_nctx, *m_text, NK_TEXT_LEFT, m_width - 30);

	nk_layout_row_dynamic(m_nctx, 40, 1);

	if (m_forceFocus)
	{
		nk_edit_focus(m_nctx, NK_EDIT_ALWAYS_INSERT_MODE);
		// XXX(itszn) not ideal but works. For some reason calling only once stoped working
		//m_forceFocus = false;
	}
	EnableInputForEdit(100, 40);

	nk_edit_string_zero_terminated(m_nctx, NK_EDIT_FIELD, m_data, sizeof(m_data) - 1, nk_filter_default);

	nk_layout_row_dynamic(m_nctx, 40, 2);

	if (nk_button_label(m_nctx, "Cancel"))
	{
		m_isOpen = false;
	}

	if (nk_button_label(m_nctx, *m_submitText))
	{
		m_submitted = true;
		m_isOpen = false;
	}

	struct nk_vec2 end_pos = nk_widget_position(m_nctx);

	if (m_shrinkWindow) {
		nk_window_set_size(m_nctx, *m_name, nk_vec2(
			m_width,
			end_pos.y - start_pos.y + 60
		));
		m_shrinkWindow = false;
	}
}