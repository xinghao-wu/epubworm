# EPUB test fixtures

These EPUBs were created specifically for Epubworm. Their prose, metadata, and
geometric artwork are original project test data and are distributed under the
repository's MIT license.

Each `.epub` archive has a matching extracted directory. The fixtures cover
different package layouts, navigation structures, image elements, image
formats, and spine behavior.

| Fixture | Coverage |
| --- | --- |
| `metadata_paths` | Root-level package discovery, metadata, percent-decoded spine and TOC paths, missing images, archive hashing, and library operations. |
| `nonlinear_spine` | Nested `OEBPS/content.opf`, relative image paths, `linear="no"` spine entries, centered content, and RGB/RGBA images. |
| `image_elements` | SVG `<image>` and HTML `<img>` elements, JPEG and transparent PNG files, percent-encoded image paths, and styled text. |
| `nested_navigation` | A nested `Book/package.opf`, three-level NCX traversal, indentation of nested entries, and removal of URL fragments. |
