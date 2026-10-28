# Adding Nibble's license to GitHub

## Draft status and intended policy

The accompanying LICENSE is a custom legal draft prepared for Juan-PrC-dev's stated policy. It has not been reviewed by a lawyer. Have a qualified lawyer review it before relying on the restrictions, especially if commercial enforcement matters. It is not MIT, Commons Clause, an OSI-approved license, or a guarantee that every part of AI-assisted work attracts copyright protection.

The draft permits free use, modification and free redistribution, including internal business use. Written permission is required to sell Nibble or modified/rebranded versions, charge for access, offer paid hosted functionality, include its code in paid products or bundles, or monetize its distribution/access with advertising. Private changes do not have to be published. Free binary redistribution is allowed with notices and the same restrictions. The policy does not claim ownership of independently implemented ideas.

The public copyright name is Juan-PrC-dev. The document contains no legal name, home address, personal email, credentials or account identifiers. Commercial-permission requests can be directed through the repository's chosen public contact method.

## Recommended repository layout

Use the contents of this Nibble folder as the repository root:

```text
LICENSE
README.md
THIRD_PARTY_NOTICES.txt
.gitignore
build.ps1
src/
assets/
licenses/
previews/                 # optional demo renders
```

The executable/test utilities can be supplied through GitHub Releases. The included .gitignore excludes local data, credentials, logs, caches, archives and compiled outputs from normal Git additions. It does not prevent someone manually uploading those files on GitHub, and does not remove files already tracked by Git. Do not upload the surrounding work folder or your Codex/AppData configuration.

## Add to an existing GitHub repository in the browser

1. Open the repository's top-level Code page.
2. Select **Add file -> Create new file**.
3. Name the file **LICENSE**, with no folder prefix.
4. Paste the complete accompanying LICENSE text into the editor. Use the custom text directly; do not select a standard MIT/GPL/Apache template or append a new MIT permission grant for Nibble.
5. Select **Commit changes...**. Use a message such as `Add Nibble source-available license`. For a default/protected branch, create a new branch and pull request, review the changes, and merge it.
6. Add the License section below to README.md. The local README supplied with this package already includes it.
7. Keep src/JSON-LICENSE.txt unchanged. Add THIRD_PARTY_NOTICES.txt and licenses/ when distributing the supplied binaries.
8. Review the resulting root LICENSE, README and third-party notices together. Confirm that any existing badges, package metadata or project descriptions do not describe Nibble itself as MIT-licensed or unrestricted open source.

GitHub's license detection supports a set of recognizable licenses. A custom license may display as Other or not have a named license badge. Keep the full text and clear README explanation; do not select an inaccurate standard license just to get a badge.

## README text

```markdown
## License

Nibble is source-available under the [Nibble Source-Available License](LICENSE).
You may use, modify and share it for free, including for internal business use.
Selling Nibble or modified versions, charging for access, offering paid hosted
versions, or including its code in a paid product or service requires prior
written permission from the applicable copyright holders. See LICENSE for the
full conditions.

Third-party components retain their own licenses; see
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).
```

## Existing releases and contributions

If your own code was previously distributed under MIT or another permissive license, adding this file does not cancel permissions already granted for those versions. Make the licensing boundary and affected releases explicit. The JSON library's MIT notice alone does not license all of Nibble under MIT.

The contribution clause asks intentionally submitted contributions to use the same terms; contributors keep ownership. Do not assume you can later sell someone else's contributions without obtaining any permissions needed from them. No copyright assignment is included.

## Sources

- GitHub: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository
- GitHub's file-creation steps: https://docs.github.com/en/repositories/working-with-files/managing-files/creating-new-files
- Copyright protects expression rather than ideas: https://www.wipo.int/en/web/copyright/faq-copyright

Nothing has been committed or uploaded to GitHub by this preparation step.
