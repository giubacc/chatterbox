{{/*
Expand the name of the chart.
*/}}
{{- define "chatterbox.name" -}}
{{- .Chart.Name }}
{{- end }}

{{/*
Create chart name and version as used by the chart label.
*/}}
{{- define "chatterbox.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Common labels
*/}}
{{- define "chatterbox.labels" -}}
helm.sh/chart: {{ include "chatterbox.chart" . }}
{{ include "chatterbox.commonSelectorLabels" . }}
{{- if .Chart.AppVersion }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end }}

{{- define "chatterbox.commonSelectorLabels" -}}
app.kubernetes.io/name: {{ include "chatterbox.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{- define "chatterbox.selectorLabels" -}}
{{ include "chatterbox.commonSelectorLabels" . }}
app.kubernetes.io/component: application
{{- end }}

{{- define "chatterbox-ui.selectorLabels" -}}
{{ include "chatterbox.commonSelectorLabels" . }}
app.kubernetes.io/component: ui
{{- end }}

{{/*
Version helpers for the image tag
*/}}
{{- define "chatterbox.image" -}}
{{- $defaulttag := printf "v%s" .Chart.Version }}
{{- $tag := default $defaulttag .Values.backend.imageTag }}
{{- $name := default "giubacc/chatterbox" .Values.backend.imageName }}
{{- $registry := default "ghcr.io" .Values.backend.imageRegistry }}
{{- printf "%s/%s:%s" $registry $name $tag }}
{{- end }}

{{- define "chatterbox-ui.image" -}}
{{- $tag := default (printf "v%s" .Chart.Version) .Values.ui.imageTag }}
{{- $name := default "giubacc/chatterbox-ui" .Values.ui.imageName }}
{{- $registry := default "ghcr.io" .Values.ui.imageRegistry }}
{{- printf "%s/%s:%s" $registry $name $tag }}
{{- end }}

{{/*
Traefik Middleware CORS name
*/}}
{{- define "chatterbox.CORSMiddlewareName" -}}
{{- $dmcn := printf "%s-%s-cors-header" .Release.Name .Release.Namespace }}
{{- $name := $dmcn }}
{{- $name }}
{{- end }}

{{/*
Backend service name
*/}}
{{- define "chatterbox.serviceName" -}}
{{- $dsn := printf "%s-%s" .Release.Name .Release.Namespace }}
{{- $name := default $dsn .Values.backend.serviceName }}
{{- $name }}
{{- end }}
