cask "i2ptorrents" do
  version "0.3.0"
  sha256 "36067c784c6a71761787bd2dd8947bf127c0b349dcd13ed401ab49a0b1e1cf99"

  url "https://github.com/MetanoicArmor/i2ptorrents-gui/releases/download/v#{version}/I2PTorrents-macOS-arm64-v#{version}.zip"
  name "I2P Torrents"
  desc "Desktop GUI for the i2pd torrent client"
  homepage "https://github.com/MetanoicArmor/i2ptorrents-gui"

  livecheck do
    url :url
    strategy :github_latest
  end

  depends_on macos: :big_sur
  depends_on arch: :arm64

  app "I2PTorrents-arm64.app", target: "I2PTorrents.app"

  postflight_steps do
    run "/usr/bin/xattr",
        args:         ["-dr", "com.apple.quarantine", "{{appdir}}/I2PTorrents.app"],
        must_succeed: false
  end

  caveats <<~EOS
    Needs a running i2pd with a type=torrents tunnel and RPC enabled.
    See #{homepage}#install-i2pd--torrents-tunnel

    CI ships Apple Silicon only. For Intel Macs, build from source.

    This build is ad-hoc signed (not Developer ID / notarized). The cask clears
    Gatekeeper quarantine after install; if macOS still blocks the app:
      xattr -dr com.apple.quarantine "#{appdir}/I2PTorrents.app"
  EOS

  zap trash: [
    "~/Library/Application Support/I2PTorrents",
    "~/Library/Preferences/org.metanoicarmor.i2ptorrents.plist",
  ]
end
