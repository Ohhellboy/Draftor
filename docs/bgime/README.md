# bgIME developer documentation website

The public Chinese prose is in `content/`. `site.config.mjs` defines the navigation and pins every engine source link to Draftor Alpha 3 (`f18939f8880e2278c231739d553fbf983c19e0f6`). No module version is invented. This site uses the existing repository's Node/static-site approach, with a small Markdown renderer and local full-text search; no service, analytics, remote font or external script is needed at runtime.

## Build and preview

Requires Node.js 22 or later. From this directory:

```sh
npm ci
npm run build
npm run check
npm run preview
```

Open `http://127.0.0.1:4178/bgime/`. The standalone preview serves only the documentation; links to the installer and guide need the combined Pages preview below. The preview server binds to loopback, does not connect to a serial port, and has no firmware operations.

Markdown supports ordinary fenced code, tables, and explicit HTML for the introductory navigation. `@source/path` expands to the exact baseline GitHub blob URL. `@doc/slug` expands to a configured `/bgime/` page. New pages must be added to the navigation; missing pages, missing baseline source files and broken fragment links fail the static check. Search indexes headings, body text and code, and matches Chinese substrings and whitespace-separated terms without a remote service.

## Coexist with the installer

GitHub Pages currently publishes the root of `gh-pages`, with `CNAME` set to `draftor.tcssn.org`. Do not change that source and **do not publish `dist/` as the entire Pages root**. It contains the documentation only.

Prepare a combined, locally reviewable tree from the current Pages commit:

```sh
git fetch origin refs/heads/gh-pages:refs/remotes/origin/gh-pages
npm run prepare:pages -- origin/gh-pages
npm run preview -- dist-pages
```

The output directory must not already exist; use an explicit second argument for another candidate, for example `npm run prepare:pages -- origin/gh-pages dist-pages-next`. The preparer exports the whole selected Pages tree, adds/replaces only `bgime/`, inserts one documentation link in the homepage footer, and regenerates the root `SITE_SHA256SUMS`. It requires `.nojekyll`, `CNAME`, the homepage, first-install page, guide and recovery page. It checks every pre-existing file outside `bgime/` for byte identity except the deliberate homepage link and root file manifest. `BUILD.json`, JavaScript bundles, firmware, recovery tools, license files, source archives and original URLs are preserved. The documented source homepage also contains the same footer link for future installer builds.

The preservation check result is written beside the output tree as `<output>-preservation.json`, not into public content. Keep it with the delivery evidence. The original Pages parent is recorded there; if the remote changes before publication, prepare again from the new head. For the repository's existing manual publication process, create a new Pages commit with that verified parent and the **combined tree**, then use a normal fast-forward push. Never force-push or reset the source checkout to the Alpha 3 tag. No automatic deployment, Pages setting change or firmware rebuild is part of these commands.

Future full installer releases must preserve this subtree or overlay a fresh docs build before generating their root manifest. Existing installer release builders remain available; they should not be pointed at the docs-only output.

## Validation and public wording

`npm run check` validates all generated local links, anchors, navigation, indexed search targets and pinned source paths. It also compiles the complete C++ sink snippet from the integration page with `-fsyntax-only` against the real headers. C++17, Git and the baseline commit must therefore be available when running this check.

Browser checks and actual Host example runs belong in the delivery record, not in the public pages. All three standalone examples disable learning. Alpha 3 product learning uses public state/notification facilities plus product-specific event qualification and scheduling. Keep that distinction when editing these pages; do not advertise the basic examples as complete learning integration.
