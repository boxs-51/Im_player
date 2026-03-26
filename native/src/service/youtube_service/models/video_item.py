from pydantic import BaseModel

class VideoItem(BaseModel):
    id: str
    title: str
    link: str

