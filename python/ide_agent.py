import sys, json, os, httpx

OPENROUTER_URL = "https://openrouter.ai/api/v1/chat/completions"
DEFAULT_MODEL = "qwen/qwen-turbo"

def get_api_key():
    key = os.environ.get("OPENROUTER_API_KEY", "").strip()
    if key:
        return key
    settings_path = os.path.join(os.path.expanduser("~"), ".config", "SimpleCASCADE", "settings.json")
    try:
        if os.path.exists(settings_path):
            with open(settings_path, "r", encoding="utf-8") as f:
                return json.load(f).get("api_key", "").strip()
    except Exception:
        pass
    return ""

async def fetch_suggestion(content: str, cursor: int, language: str, path: str) -> str:
    headers = {"HTTP-Referer": "http://localhost", "X-Title": "SimpleCASCADE IDE"}
    api_key = get_api_key()
    if api_key:
        headers["Authorization"] = f"Bearer {api_key}"
    prompt = (
        "You are a code completion engine. Return ONLY the continuation text that should be inserted at the cursor.\n"
        f"Language: {language}\n"
        f"File: {path}\n"
        "Provide short, syntactically correct code.")
    user = content[:cursor] + "<CURSOR>" + content[cursor:]
    payload = {
        "model": DEFAULT_MODEL,
        "messages": [
            {"role": "system", "content": prompt},
            {"role": "user", "content": user}
        ],
        "temperature": 0.2,
        "max_tokens": 128,
    }
    async with httpx.AsyncClient(timeout=20.0) as client:
        r = await client.post(OPENROUTER_URL, json=payload, headers=headers)
        r.raise_for_status()
        data = r.json()
        return data["choices"][0]["message"]["content"]

def main():
    raw = sys.stdin.read()
    try:
        body = json.loads(raw)
    except Exception:
        print("", end="")
        return
    content = body.get("content", "")
    cursor = int(body.get("cursor", 0))
    language = body.get("language", "")
    path = body.get("path", "")
    try:
        import asyncio
        text = asyncio.run(fetch_suggestion(content, cursor, language, path))
        print(text.strip(), end="")
    except Exception:
        print("", end="")

if __name__ == "__main__":
    main()

