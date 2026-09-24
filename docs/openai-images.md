# Local image generation

Run from the repository root with Python 3; no packages are required.
Credentials stay on your machine and are not bundled with the game.

Create an OpenAI project API key at https://platform.openai.com/api-keys
and configure API billing. Organization verification may be required.
Store the key alone in `.tools/openai_api_key` (ignored by Git), or set
`OPENAI_API_KEY` in the environment, which takes precedence.

To securely enter or replace the local key in Bash without putting it in shell history:

```bash
mkdir -p .tools
read -rsp 'OpenAI API key: ' openai_image_key
printf '\n'
(umask 077; printf '%s\n' "$openai_image_key" > .tools/openai_api_key)
chmod 600 .tools/openai_api_key
unset openai_image_key
```

Check authentication without generating an image:

```bash
python3 scripts/openai_images.py check
```

Generate a PNG (uses API credits):

```bash
python3 scripts/openai_images.py generate \
  "Isometric desert watchtower, pixel art, isolated game asset" \
  --transparent --out .tools/generated/watchtower.png
```

Defaults: `gpt-image-2.5-flare`, low quality, 1024×1024. Use `--model`,
`--quality`, and `--size` to change these. Existing output files are never
overwritten. Review generated images before copying them into game assets.
The authentication check does not verify image generation permissions or billing.

HTTP 401 means the key is invalid; 403 means access is denied; 429 indicates
a quota or rate limit issue. Replace credentials exposed in chat with a fresh key.

Official documentation: https://developers.openai.com/api/docs/guides/image-generation
