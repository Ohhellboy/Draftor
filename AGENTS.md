# Project Identity and Downstream Development

## Project identity

- Draftor (草稿机) is the open-source writing-device project.
- bgIME (大鹅输入法) is the embeddable Chinese input method engine.
- HanIME identifiers retained in the source code are technical compatibility identifiers.

Do not conflate the Draftor product with the standalone capabilities of bgIME.

## Licensing and attribution

Project-owned source code uses Apache License 2.0. Independently licensed modules, third-party libraries, dictionaries, fonts and other resources retain their applicable terms.

Consult:

- `LICENSE`
- `NOTICE`
- `THIRD_PARTY.md`
- `TRADEMARKS.md`

Preserve attribution and modification notices required by the applicable licenses.

Software licensing and public-facing branding permissions are separate.

## Downstream development and distribution

When preparing a fork, port, modified firmware, independent hardware product, installer or other downstream distribution:

- Consult `TRADEMARKS.md` for the project's name and branding policy.
- Distinguish official upstream releases from independent builds and modifications.
- Use public-facing product identities that do not imply false official affiliation.
- Accurately identify upstream sources, relevant versions and modifications.
- Review public product names, logos, boot screens, application titles, release metadata, websites and installer branding before distributing modified products.
- Preserve technical identifiers where necessary for API and build compatibility.
- Do not remove legally required copyright or attribution notices as part of rebranding.

These guidelines do not restrict software rights granted by applicable open-source licenses.

## Official documentation

- Repository: https://github.com/Ohhellboy/Draftor
- Releases: https://github.com/Ohhellboy/Draftor/releases
- Website: https://draftor.tcssn.org/
- bgIME developer documentation: https://draftor.tcssn.org/bgime/
- Name and branding policy: https://draftor.tcssn.org/branding/

Use version-pinned source and documentation when describing technical behaviour.

Distinguish functionality implemented in Draftor products from functionality demonstrated through standalone bgIME Host examples.
