#pragma once

#include "Sqex.h"
#include "Sqex_FontCsv_ModifiableFontCsvStream.h"

namespace Sqex::FontCsv {

	struct GlyphMeasurement {
		bool empty = true;
		SSIZE_T left = 0;
		SSIZE_T top = 0;
		SSIZE_T right = 0;
		SSIZE_T bottom = 0;
		SSIZE_T offsetX = 0;

		void AdjustToIntersection(GlyphMeasurement& r, SSIZE_T srcWidth, SSIZE_T srcHeight, SSIZE_T destWidth, SSIZE_T destHeight);
		SSIZE_T Width() const { return right - left; }
		SSIZE_T Height() const { return bottom - top; }
	};

	class SeCompatibleFont {
		mutable GlyphMeasurement m_maxBoundingBox = { true };

	public:
		SeCompatibleFont() = default;
		virtual ~SeCompatibleFont() = default;

		[[nodiscard]] virtual bool HasCharacter(char32_t) const = 0;
		[[nodiscard]] virtual SSIZE_T GetCharacterWidth(char32_t c) const = 0;
		[[nodiscard]] virtual float Size() const = 0;
		[[nodiscard]] virtual const std::vector<char32_t>& GetAllCharacters() const = 0;
		[[nodiscard]] virtual GlyphMeasurement MaxBoundingBox() const;
		[[nodiscard]] virtual uint32_t Ascent() const = 0;
		[[nodiscard]] virtual uint32_t Descent() const = 0;
		[[nodiscard]] virtual uint32_t Height() const { return Ascent() + Descent(); }
		[[nodiscard]] virtual const std::map<std::pair<char32_t, char32_t>, SSIZE_T>& GetKerningTable() const = 0;
		[[nodiscard]] virtual SSIZE_T GetKerning(char32_t l, char32_t r, SSIZE_T defaultOffset = 0) const;

		[[nodiscard]] virtual GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, char32_t c) const = 0;
		[[nodiscard]] virtual GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, const std::u32string& s) const;
		[[nodiscard]] virtual GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, const std::string& s) const {
			return Measure(x, y, ToU32(s));
		}
	};

	class SeFont : public virtual SeCompatibleFont {
		struct Implementation;
		const std::unique_ptr<Implementation> m_pImpl;

	public:
		SeFont(std::shared_ptr<const ModifiableFontCsvStream> stream);
		~SeFont() override;

		[[nodiscard]] bool HasCharacter(char32_t) const override;
		[[nodiscard]] SSIZE_T GetCharacterWidth(char32_t c) const override;
		[[nodiscard]] float Size() const override;
		[[nodiscard]] const std::vector<char32_t>& GetAllCharacters() const override;
		[[nodiscard]] uint32_t Ascent() const override;
		[[nodiscard]] uint32_t Descent() const override;
		[[nodiscard]] const std::map<std::pair<char32_t, char32_t>, SSIZE_T>& GetKerningTable() const override;

		using SeCompatibleFont::Measure;
		[[nodiscard]] static GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, const FontTableEntry& entry);
		[[nodiscard]] GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, char32_t c) const override;

	protected:
		[[nodiscard]] const ModifiableFontCsvStream& GetStream() const;
	};

	class CascadingFont : public virtual SeCompatibleFont {
		struct Implementation;
		const std::unique_ptr<Implementation> m_pImpl;

	public:
		CascadingFont(std::vector<std::shared_ptr<SeCompatibleFont>> fontList);
		CascadingFont(std::vector<std::shared_ptr<SeCompatibleFont>> fontList, float normalizedSize, uint32_t ascent, uint32_t descent);
		~CascadingFont() override;

		[[nodiscard]] bool HasCharacter(char32_t) const override;
		[[nodiscard]] SSIZE_T GetCharacterWidth(char32_t c) const override;
		[[nodiscard]] float Size() const override;
		[[nodiscard]] const std::vector<char32_t>& GetAllCharacters() const override;
		[[nodiscard]] uint32_t Ascent() const override;
		[[nodiscard]] uint32_t Descent() const override;
		[[nodiscard]] const std::map<std::pair<char32_t, char32_t>, SSIZE_T>& GetKerningTable() const override;

		using SeCompatibleFont::Measure;
		[[nodiscard]] GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, char32_t c) const override;
		[[nodiscard]] GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, const std::u32string& s) const override;

	protected:
		[[nodiscard]] const std::vector<std::shared_ptr<SeCompatibleFont>>& GetFontList() const;
	};

	class GdiFont : public virtual SeCompatibleFont {
		struct Implementation;
		const std::unique_ptr<Implementation> m_pImpl;

	public:
		GdiFont(const LOGFONTW&);
		~GdiFont() override;

		[[nodiscard]] bool HasCharacter(char32_t) const override;
		[[nodiscard]] SSIZE_T GetCharacterWidth(char32_t c) const override;
		[[nodiscard]] float Size() const override;
		[[nodiscard]] const std::vector<char32_t>& GetAllCharacters() const override;
		[[nodiscard]] uint32_t Ascent() const override;
		[[nodiscard]] uint32_t Descent() const override;
		[[nodiscard]] const std::map<std::pair<char32_t, char32_t>, SSIZE_T>& GetKerningTable() const override;

		using SeCompatibleFont::Measure;
		[[nodiscard]] GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, char32_t c) const override;

	protected:
		class DeviceContextWrapper {
			const HDC m_hdc;
			const Utils::CallOnDestruction m_hdcRelease;

			const HFONT m_hFont;
			const Utils::CallOnDestruction m_fontRelease;
			const HFONT m_hPrevFont;
			const Utils::CallOnDestruction m_prevFontRevert;
			
			std::vector<uint8_t> m_readBuffer;

		public:
			const TEXTMETRICW Metrics;
			DeviceContextWrapper(const LOGFONTW& logfont);
			~DeviceContextWrapper();

			[[nodiscard]] HDC GetDC() const { return m_hdc; }
			[[nodiscard]] SSIZE_T GetCharacterWidth(char32_t c) const;
			[[nodiscard]] GlyphMeasurement Measure(SSIZE_T x, SSIZE_T y, char32_t c) const;

			std::pair<const std::vector<uint8_t>*, GlyphMeasurement> Draw(SSIZE_T x, SSIZE_T y, char32_t c);
		};

		class DeviceContextWrapperContext {
			const GdiFont* m_font;
			std::unique_ptr<DeviceContextWrapper> m_wrapper;

		public:
			DeviceContextWrapperContext() :
				m_font(nullptr), m_wrapper(nullptr){}
			DeviceContextWrapperContext(const GdiFont* font, std::unique_ptr<DeviceContextWrapper> wrapper)
				: m_font(font)
				, m_wrapper(std::move(wrapper)) {
			}
			DeviceContextWrapperContext(const DeviceContextWrapperContext&) = delete;
			DeviceContextWrapperContext(DeviceContextWrapperContext&& r) noexcept
				: m_font(r.m_font)
				, m_wrapper(std::move(r.m_wrapper)) {
				r.m_font = nullptr;
			}
			DeviceContextWrapperContext& operator=(const DeviceContextWrapperContext&) = delete;
			DeviceContextWrapperContext& operator=(DeviceContextWrapperContext&& r) noexcept {
				m_font = r.m_font;
				m_wrapper = std::move(r.m_wrapper);
				r.m_font = nullptr;
				return *this;
			}

			DeviceContextWrapper* operator->() const {
				return m_wrapper.get();
			}

			~DeviceContextWrapperContext() {
				if (m_wrapper)
					m_font->FreeDeviceContext(std::move(m_wrapper));
			}
		};

		DeviceContextWrapperContext AllocateDeviceContext() const;
		void FreeDeviceContext(std::unique_ptr<DeviceContextWrapper> wrapper) const;
	};
}
