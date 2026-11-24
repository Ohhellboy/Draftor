# R12 public installer

From the web-installer source directory run `npm ci` and `npm run build:r12-public -- dist-r12-public`. The optional second argument is a directory containing the R11 GitHub Pages site. It defaults to `dist-r11-public`. Obtain that directory by exporting the root tree of Pages commit `8ca75a4b71d104407098d54001bcbae8dd38e898` from https://github.com/Ohhellboy/Draftor. The builder verifies its pinned site manifest and copies only its listed files.

The R12 app0 installer uses the same native serial entry, exclusive write grant and installation session as hardware-tested commit `197e5613df6a5a58d052c9fcce17beb168f8dd38`. The public profile permits the exact HTTPS site origin. The former homepage runs at `first-install.html` with its original script, firmware and recovery tool at their existing paths. The downloadable source archive includes the pinned factory reference required by the R12 implementation and its upstream license.

Serve the contents of the output directory at the root of the existing Pages site. Retain the previous Pages commit as the parent for rollback. Windows desktop Chrome and Edge remain eligible. Real Windows hardware verification is pending.
