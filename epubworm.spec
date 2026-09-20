# vim: syntax=spec

Name:           epubworm
Version:        {{{ git_dir_version }}}
Release:        1%{?dist}
Summary:        Image-capable terminal EPUB reader

# See THIRD_PARTY_LICENSES for the license breakdown.
License:        MIT AND Zlib AND Unlicense
URL:            https://github.com/xinghao-wu/epubworm
VCS:            {{{ git_dir_vcs }}}
Source:         {{{ git_dir_pack }}}

BuildRequires:  gcc-c++
BuildRequires:  libasan
BuildRequires:  make

Provides:       bundled(base64)
Provides:       bundled(miniz) = 1.15
Provides:       bundled(miniz-cpp)
Provides:       bundled(stb_image) = 2.30
Provides:       bundled(tinyxml2) = 11.0.0

%description
Epubworm is a minimal terminal EPUB reader with inline image display using the
Kitty Graphics Protocol, wide-character support, mouse controls, Vim-like
keybindings, and persistent reading progress.

%prep
{{{ git_dir_setup_macro }}}

%build
%set_build_flags
%make_build

%install
%make_install \
    PREFIX=%{_prefix} \
    DOCDIR=%{_defaultlicensedir}/%{name}

%check
%make_build test

%files
%license %{_defaultlicensedir}/%{name}/LICENSE
%license %{_defaultlicensedir}/%{name}/THIRD_PARTY_LICENSES
%doc README.md
%{_bindir}/%{name}
%{_datadir}/bash-completion/completions/%{name}
%{_datadir}/fish/vendor_completions.d/%{name}.fish
%{_datadir}/zsh/site-functions/_%{name}

%changelog
* Sat Sep 19 2026 Harry Wu <310783731+xinghao-wu@users.noreply.github.com>
- Initial package
