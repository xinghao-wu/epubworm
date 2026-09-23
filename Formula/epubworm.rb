class Epubworm < Formula
  desc "Fast, image-capable terminal EPUB reader"
  homepage "https://github.com/xinghao-wu/epubworm"
  license all_of: ["MIT", "Zlib", "Unlicense"]
  head "https://github.com/xinghao-wu/epubworm.git", branch: "main"

  def install
    system "make", "install", "CXX=#{ENV.cxx}", "PREFIX=#{prefix}"
  end

  test do
    ENV["XDG_CONFIG_HOME"] = testpath/"config"
    ENV["XDG_DATA_HOME"] = testpath/"data"

    assert_match "Line length set to 72", shell_output("#{bin}/epubworm set-line-length 72")
    assert_match 'chars="72"', (testpath/"config/epubworm/conf.xml").read
    assert_path_exists testpath/"data/epubworm/library.xml"
  end
end
