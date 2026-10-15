cask "i2ptorrents" do
  version "0.2.7"

  on_arm do
    sha256 "REPLACE_AFTER_RELEASE"

    url "https://github.com/MetanoicArmor/i2ptorrents-gui/releases/download/v#{version}/I2PTorrents-macOS-arm64-v#{version}.zip"
    app "I2PTorrents-arm64.app", target: "I2PTorrents.app"
  end
  on_intel do
    sha256 "REPLACE_AFTER_RELEASE"

    url "https://github.com/MetanoicArmor/i2ptorrents-gui/releases/download/v#{version}/I2PTorrents-macOS-x64-v#{version}.zip"
    app "I2PTorrents-x64.app", target: "I2PTorrents.app"
  end

  name "I2P Torrents"
  desc "Desktop GUI for the i2pd torrent client"
  homepage "https://github.com/MetanoicArmor/i2ptorrents-gui"

  livecheck do
    url :url
    strategy :github_latest
  end

  depends_on macos: ">= :big_sur"

  postflight do
    system_command "/usr/bin/xattr",
                   args: ["-dr", "com.apple.quarantine", "#{appdir}/I2PTorrents.app"],
                   sudo: false
  end

  caveats <<~EOS
    Needs a running i2pd with a type=torrents tunnel and RPC enabled.
    See #{homepage}#install-i2pd--torrents-tunnel

    This build is ad-hoc signed (not Developer ID / notarized). The cask clears
    Gatekeeper quarantine after install; if macOS still blocks the app:
      xattr -dr com.apple.quarantine "#{appdir}/I2PTorrents.app"
  EOS

  zap trash: [
    "~/Library/Application Support/I2PTorrents",
    "~/Library/Preferences/org.metanoicarmor.i2ptorrents.plist",
  ]
end
