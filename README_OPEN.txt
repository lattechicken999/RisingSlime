RisingSlime — 에디터 여는 방법 (중요)

금지: Epic Games Launcher에서 "언리얼 엔진 5.8" → Launch 만 누르기
  → uproject 없이 기동 → AutoLoad가 너무 늦어 RaisingSlime 모듈을 못 찾음
  → "could not be compiled" / "built with a different engine version" 다이얼로그

반드시 아래 중 하나로 여세요:

1) (권장) D:\UnrealGames\RisingSlime\OpenEditor.bat 더블클릭
2) D:\UnrealGames\RisingSlime\RisingSlime.uproject 더블클릭
3) 탐색기에서 uproject 우클릭 → 연결 프로그램 → Unreal Editor 5.8

다이얼로그가 뜨면:
- "Rebuild?" → No
- "Proceed anyway?" → No  (진행하지 마세요)
- 위 1)~3)으로 다시 여세요

엔진/모듈 BuildId는 둘 다 55116800으로 일치합니다. ABI 불일치가 아니라
"Epic Launch = 프로젝트 경로 미전달"이 원인입니다.
